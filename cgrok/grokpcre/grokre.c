#include <pcre.h>
#include <assert.h>
#include <string.h> 
#include <stdlib.h>
#include <stdio.h>
#include <search.h>

#include "stringhelper.h"

typedef struct grok_pattern {
  const char *name;
  char *regexp;
} grok_pattern_t;

typedef struct grok_capture {
  const char *name;
  int id;
  const char *pattern;
  const char *predicate;
} grok_capture_t;

typedef struct grok {
  /* tree of grok_pattern objects */
  void *patterns;
  
  /* These are initialized when grok_compile is called */
  pcre *re;
  const char *pattern;
  char *full_pattern;
  int *capture_vector; /* XXX: should rename pcre_capture_vector */
  int num_captures; /* XXX: should rename pcre_num_captures */
  void *captures_by_id;
  void *captures_by_name;

  /* PCRE pattern compilation errors */
  const char *pcre_errptr;
  int pcre_erroffset;
} grok_t;

typedef struct grok_match {
  grok_t *grok;
  const char *subject;
} grok_match_t;

/* global, static variables */

/* pattern to match %{FOO:BAR} */
#define PATTERN_REGEX "%{" \
                        "(?<name>" \
                          "(?<pattern>[A-z0-9._-]+)" \
                          "(?::(?<subname>[A-z0-9._-]+))?" \
                          "(?<predicate>\\s*(?:=[<>=~]|[<>])\\s*[^}]+)?" \
                        ")" \
                      "}"
#define CAPTURE_ID_LEN 4

static pcre *g_pattern_re = NULL;
static int g_pattern_num_captures = 0;
static int g_cap_name = 0;
static int g_cap_pattern = 0;
static int g_cap_subname = 0;
static int g_cap_predicate = 0;

void pwalk(const void *node, VISIT visit, int nodelevel);

/* public functions */
void grok_init(grok_t *grok);
void grok_free(grok_t *grok);
void grok_patterns_import_from_file(grok_t *grok, const char *filename);
void grok_patterns_import_from_string(grok_t *grok, char *buffer);

int grok_compile(grok_t *grok, const char *pattern);
int grok_exec(grok_t *grok, const char *text, grok_match_t *gm);

/* internal functions */
static void grok_pattern_add(grok_t *grok, grok_pattern_t *pattern);
static char *grok_pattern_expand(grok_t *grok);
static grok_pattern_t *grok_pattern_find(grok_t *grok, const char *pattern_name);
static int grok_pcre_callout(pcre_callout_block *);

static void grok_capture_add(grok_t *grok, int capture_id, const char *pattern_name);
static void grok_capture_add_predicate(grok_t *grok, int capture_id,
                                       const char *predicate);

static void grok_match_get_named_capture(grok_match_t *gm, const char *name,
                                         grok_capture_t **gct);

static int grok_pattern_cmp_name(const void *a, const void *b);
static int grok_capture_cmp_id(const void *a, const void *b);
static int grok_capture_cmp_name(const void *a, const void *b);

static void _pattern_parse_string(const char *line, grok_pattern_t *pattern_ret);

void grok_init(grok_t *grok) {
  pcre_callout = grok_pcre_callout;

  grok->patterns = NULL;
  grok->re = NULL;
  grok->pattern = NULL;
  grok->full_pattern = NULL;
  grok->capture_vector = NULL;
  grok->num_captures = 0;
  grok->captures_by_id = 0;
  grok->captures_by_name = 0;
  grok->pcre_errptr = NULL;
  grok->pcre_erroffset = 0;

  if (g_pattern_re == NULL) {
    g_pattern_re = pcre_compile(PATTERN_REGEX, 0, 
                                &grok->pcre_errptr,
                                &grok->pcre_erroffset,
                                NULL);
    if (g_pattern_re == NULL) {
      fprintf(stderr, "Internal compiler error: %s\n", grok->pcre_errptr);
      fprintf(stderr, "Regexp: %s\n", PATTERN_REGEX);
      fprintf(stderr, "Position: %d\n", grok->pcre_erroffset);
    }

    pcre_fullinfo(g_pattern_re, NULL, PCRE_INFO_CAPTURECOUNT,
                  &g_pattern_num_captures);
    g_pattern_num_captures++; /* include the 0th group */
    g_cap_name = pcre_get_stringnumber(g_pattern_re, "name");
    g_cap_pattern = pcre_get_stringnumber(g_pattern_re, "pattern");
    g_cap_subname = pcre_get_stringnumber(g_pattern_re, "subname");
    g_cap_predicate = pcre_get_stringnumber(g_pattern_re, "predicate");
  }
}

void grok_free(grok_t *grok) {
  /* twalk grok->patterns and free them? */

  if (grok->re != NULL)
    pcre_free(grok->re);

  if (grok->full_pattern != NULL)
    free(grok->full_pattern);

  if (grok->capture_vector != NULL)
    free(grok->capture_vector);
}

void grok_patterns_import_from_file(grok_t *grok, const char *filename) {
  FILE *patfile = NULL;
  size_t filesize = 0;
  size_t bytes = 0;
  char *buffer = NULL;

  patfile = fopen(filename, "r");
  if (patfile == NULL) {
    fprintf(stderr, "Unable to open '%s' for reading\n", filename);
    perror("Error: ");
    return;
  }

  fseek(patfile, 0, SEEK_END);
  filesize = ftell(patfile);
  fseek(patfile, 0, SEEK_SET);
  buffer = calloc(1, filesize);
  memset(buffer, 0, filesize);
  bytes = fread(buffer, 1, filesize, patfile);
  if (bytes != filesize) {
    fprintf(stderr, "Expected %zd bytes, but read %zd.", filesize, bytes);
    return;
  }

  grok_patterns_import_from_string(grok, buffer);

  free(buffer);
  fclose(patfile);
}

void grok_patterns_import_from_string(grok_t *grok, char *buffer) {
  char *tokctx = NULL;
  char *tok = NULL;
  char *strptr = NULL;
  char *dupbuf = NULL;

  dupbuf = strdup(buffer);
  strptr = dupbuf;

  while ((tok = strtok_r(strptr, "\n", &tokctx)) != NULL) {
    grok_pattern_t tmp;
    grok_pattern_t **result;
    strptr = NULL;

    /* skip leading whitespace */
    tok += strspn(tok, " \t");

    /* If first non-whitespace is a '#', then this is a comment. */
    if (*tok == '#') continue;

    _pattern_parse_string(tok, &tmp);
    grok_pattern_add(grok, &tmp);
    result = tfind(&tmp, &(grok->patterns), grok_pattern_cmp_name);
    assert(result != NULL);
    //printf("%s vs %s\n", (*result)->name, tmp.name);
    assert(!strcmp((*result)->name, tmp.name));
  }


  free(dupbuf);
}

void grok_pattern_add(grok_t *grok, grok_pattern_t *pattern) {
  /* tsearch(3) claims it adds a node if it does not exist */
  grok_pattern_t *newpattern = NULL;
  const void *ret = NULL;

  newpattern = calloc(1, sizeof(grok_pattern_t));
  newpattern->name = strdup(pattern->name);
  newpattern->regexp = strdup(pattern->regexp);

  ret = tsearch(newpattern, &(grok->patterns), grok_pattern_cmp_name);
  if (ret == NULL)
    fprintf(stderr, "Failed adding pattern '%s'\n", newpattern->name);
}

int grok_compile(grok_t *grok, const char *pattern) {
  grok->pattern = pattern;
  grok->full_pattern = grok_pattern_expand(grok);

  grok->re = pcre_compile(grok->full_pattern, 0, 
                          &grok->pcre_errptr, &grok->pcre_erroffset,
                          NULL);

  if (grok->re == NULL) {
    printf("Regex error: %s\n", grok->pcre_errptr);
    return 1;
  }

  pcre_fullinfo(grok->re, NULL, PCRE_INFO_CAPTURECOUNT, &grok->num_captures);
  grok->num_captures++; /* include the 0th group */
  grok->capture_vector = calloc(3 * grok->num_captures, sizeof(int));

  return 0;
}

int grok_exec(grok_t *grok, const char *text, grok_match_t *gm) {
  int ret;
  pcre_extra pce;
  pce.flags = PCRE_EXTRA_CALLOUT_DATA;
  pce.callout_data = grok;

  ret = pcre_exec(grok->re, &pce, text, strlen(text), 0, 0,
                  grok->capture_vector, grok->num_captures * 3);
  if (ret < 0) {
    switch (ret) {
      case PCRE_ERROR_NOMATCH:
        break;
      case PCRE_ERROR_NULL:
        printf("Null error, one of the arguments was null?\n");
        break;
      case PCRE_ERROR_BADOPTION:
        printf("badoption\n");
        break;
      case PCRE_ERROR_BADMAGIC:
        printf("badmagic\n");
        break;
    }
    return ret;
  }

  if (gm != NULL) {
    gm->grok = grok;
    gm->subject = text;
  }

  return ret;
}

/* XXX: This method is pretty long; split it up? */
char *grok_pattern_expand(grok_t *grok) {
  int capture_id = 0;
  int offset = 0;
  int *capture_vector = NULL;

  int full_len = -1;
  int full_size = -1;
  char *full_pattern = NULL;
  char capture_id_str[CAPTURE_ID_LEN + 1];

  capture_vector = calloc(3 * g_pattern_num_captures, sizeof(int));
  full_len = strlen(grok->pattern);
  full_size = full_len + 1;
  full_pattern = calloc(1, full_size);
  strncpy(full_pattern, grok->pattern, full_len);

  const char *patname = NULL;

  while (pcre_exec(g_pattern_re, NULL, full_pattern, full_len, offset, 
                   0, capture_vector, g_pattern_num_captures * 3) >= 0) {
    int start, end, matchlen;
    grok_pattern_t *gpt;

    start = capture_vector[0];
    end = capture_vector[1];
    matchlen = end - start;

    pcre_get_substring(full_pattern, capture_vector, g_pattern_num_captures,
                       g_cap_pattern, &patname);
    gpt = grok_pattern_find(grok, patname);
    if (gpt == NULL) {
      offset = end;
    } else {
      int regexp_len = strlen(gpt->regexp);
      snprintf(capture_id_str, CAPTURE_ID_LEN + 1,
               "%0*x", CAPTURE_ID_LEN, capture_id);

      /* Add this capture to the list of captures */
      grok_capture_add(grok, capture_id, patname);

      /* Invariant, full_pattern actual len must always be full_len */
      assert(strlen(full_pattern) == full_len);

      /* Replace %{FOO} with (?<>). '5' is strlen("(?<>)") */
      substr_replace(&full_pattern, &full_len, &full_size,
                     start, end, "(?<>)", 5);

      /* if a predicate was given, add (?C1) to callout when the match is made,
       * so we can test it further */
      if (capture_vector[g_cap_predicate * 2] >= 0) {
        /* 5 == strlen("(?<>)") */
        const char *predicate = NULL;
        substr_replace(&full_pattern, &full_len, &full_size,
                       start + 5, -1, "(?C1)", 5);
        pcre_get_substring(full_pattern, capture_vector, g_pattern_num_captures,
                           g_cap_predicate, &predicate);
        grok_capture_add_predicate(grok, capture_id, predicate);
      }

      /* Insert the capture id into (?<FOO>) */
      substr_replace(&full_pattern, &full_len, &full_size,
                     start + 3, -1,
                     capture_id_str, CAPTURE_ID_LEN);

      /* Insert the pattern into (?<FOO>pattern) */
      /* 3 = '(?<', 4 = strlen(capture_id_str), 1 = ")" */
      substr_replace(&full_pattern, &full_len, &full_size, 
                     start + 3 + CAPTURE_ID_LEN + 1, -1, 
                     gpt->regexp, regexp_len);


      /* Invariant, full_pattern actual len must always be full_len */
      assert(strlen(full_pattern) == full_len);
      
      /* Move offset to the start of the regexp pattern we just injected.
       * This is so when we iterate again, we can process this new pattern
       * to see if the regexp included itself any %{FOO} things */
      offset = start;
      capture_id++;
    }

    if (patname != NULL) {
      pcre_free_substring(patname);
      patname = NULL;
    }
  }
  printf("%s\n", full_pattern);

  return full_pattern;
}

grok_pattern_t *grok_pattern_find(grok_t *grok, const char *pattern_name) {
  grok_pattern_t key;
  grok_pattern_t **result;

  key.name = pattern_name;
  key.regexp = NULL;

  result = (grok_pattern_t **) tfind(&key, &(grok->patterns), grok_pattern_cmp_name);
  if (result == NULL)
    return NULL;
  return *result;
}

void _pattern_parse_string(const char *line, grok_pattern_t *pattern_ret) {
  char *linedup = strdup(line);
  char *name = linedup;
  char *regexp = NULL;
  size_t offset;

  /* Find the first whitespace */
  offset = strcspn(line, " \t");
  linedup[offset] = '\0';
  offset += strspn(linedup + offset, " \t");
  regexp = linedup + offset + 1;

  pattern_ret->name = strdup(name);
  pattern_ret->regexp = strdup(regexp);

  free(linedup);
}

int main(int argc, const char * const *argv) {
  grok_t grok;

  grok_init(&grok);
  grok_patterns_import_from_file(&grok, "pcregrok_patterns");

  if (argc != 2) {
    printf("Usage: $0 <pattern>\n");
    return 1;
  }

  grok_compile(&grok, argv[1]);

  if (1) { /* read from stdin, apply the given pattern to it */
    int ret;
    grok_match_t gm;
    char buffer[4096];
    FILE *fp;
    fp = stdin;
    while (!feof(fp)) {
      fgets(buffer, 4096, fp);
      ret = grok_exec(&grok, buffer, &gm);
      if (ret >= 0) {
        grok_capture_t *gct = NULL;
        grok_match_get_named_capture(&gm, "MAC", &gct);
        printf("Found: %d\n", gct != NULL);
        if (gct != NULL) {
          const char *p;
          int num;
          char cap_id[CAPTURE_ID_LEN + 1];
          snprintf(cap_id, CAPTURE_ID_LEN + 1, "%0*x", CAPTURE_ID_LEN, gct->id);
          printf("Name: %s\n", gct->name);
          num = pcre_get_stringnumber(gm.grok->re, cap_id);
          pcre_get_substring(gm.subject, gm.grok->capture_vector,
                             gm.grok->num_captures, num, &p);
          printf("Value: %s\n", p);
          pcre_free_substring(p);
        }
      }
    }
  }
  return 0;
}

void pwalk(const void *node, VISIT visit, int nodelevel) {
  grok_pattern_t *pat = *(grok_pattern_t **)node;
  switch (visit) {
    case preorder:
      break;
    case postorder:
      printf("postorder: %d: %s\n", nodelevel, pat->name);
      break;
    case endorder:
      break;
    case leaf:
      printf("leaf: %d: %s\n", nodelevel, pat->name);
      break;
  }
}

static int grok_pcre_callout(pcre_callout_block *pcb) {
  printf("callout\n");
  return 0;
}

static void grok_capture_add(grok_t *grok, int capture_id,
                             const char *pattern_name) {
  grok_capture_t key;

  key.id = capture_id;
  if (tfind(&key, &(grok->captures_by_id), grok_capture_cmp_id) == NULL) {
    grok_capture_t *gcap = calloc(1, sizeof(grok_capture_t));
    gcap->id = capture_id;
    gcap->name = strdup(pattern_name);
    gcap->predicate = NULL;
    gcap->pattern = NULL;
    tsearch(gcap, &(grok->captures_by_id), grok_capture_cmp_id);
    tsearch(gcap, &(grok->captures_by_name), grok_capture_cmp_name);
  }
}

static void grok_capture_add_predicate(grok_t *grok, int capture_id,
                                       const char *predicate) {
  grok_capture_t key;
  void *result;
  grok_capture_t *gcap;

  key.id = capture_id;
  result = tsearch(&key, &(grok->captures_by_id), grok_capture_cmp_id);
  assert(result != NULL);

  gcap = *(grok_capture_t **)result;
  gcap->predicate = strdup(predicate);
}

static int grok_pattern_cmp_name(const void *a, const void *b) {
  grok_pattern_t *ga = (grok_pattern_t *)a;
  grok_pattern_t *gb = (grok_pattern_t *)b;
  return strcmp(ga->name, gb->name);
}

static int grok_capture_cmp_id(const void *a, const void *b) {
  return ((grok_capture_t *)a)->id - ((grok_capture_t *)b)->id;
}

static int grok_capture_cmp_name(const void *a, const void *b) {
  return strcmp(((grok_capture_t *)a)->name,
                ((grok_capture_t *)b)->name);
}

static void grok_match_get_named_capture(grok_match_t *gm, const char *name,
                                         grok_capture_t **gct) {
  grok_capture_t key;
  void *result;

  key.name = name;
  result = tsearch(&key, &(gm->grok->captures_by_name), grok_capture_cmp_name);
  if (result == NULL)
    *gct = NULL;

  *gct = *(grok_capture_t **)result;
}