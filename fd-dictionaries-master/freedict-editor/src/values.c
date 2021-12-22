/** @file
 * @brief Implementation of a list of string labels and corresponding string
 * values
 *
 * These lists are to be used as option menu contents and TEI typologies.
 */

#include "values.h"

// these are initialized by on_app1_show() in callbacks.c
Values *pos_values,
       *num_values,
       *domain_values,
       *register_values,
       *xr_values,
       *gen_values;

/// Frees a GSList, but contrary to g_slist_free() frees the data of the elements first
void my_g_slist_free_all(GSList *g)
{
  void my_g_slist_free_list_element(gpointer data, gpointer user_data)
  { g_free(data); }
  g_slist_foreach(g, my_g_slist_free_list_element, NULL);
  g_slist_free(g);
}


/// Convert a Values array into a GSList
/**
 * The labels and values of the elements of the Values array
 * are concatenated using TAB characters.
 *
 * @retval Pointer to a newly allocated GSList
 *
 */
GSList *Values2GSList(const Values *values)
{
  GSList *g = NULL;
  int i = 0;
  while(values && values->label && values->value)
  {
    GSList *g_old = g;
    int lenlabel = strlen(values->label);
    char *data = (char *) g_malloc(lenlabel+strlen(values->value)+1);
    strncpy(data, values->label, lenlabel);
    char *data1 = data + lenlabel;
    *data1 = '\t';
    strcpy(data1+1, values->value);
    g = g_slist_prepend(g_old, data);
    if(!g)
    {
      my_g_slist_free_all(g_old);
      return NULL;
    }
    values++;
    i++;
  }
  return g;
}

/// Convert a GSList to a Values array
/**
 * The labels and values of the elements of the Values array are made by
 * splitting the elements of the GSList at TAB characters.
 *
 * The caller is responsible for freeing the Values array, as well as all
 * its elements' labels and values.
 *
 * @retval Pointer to a newly allocated Values array
 *
 */
Values *GSList2Values(GSList *g)
{
  g_return_val_if_fail(g, NULL);
  guint l = g_slist_length(g);
  Values *v0 = (Values *) g_malloc(sizeof(Values) * (l+1));
  g_return_val_if_fail(v0, NULL);
  Values *v = v0;
  g_printerr(_("GSList2Values: length=%i sizeof(Values)=%i\n"), l, sizeof(Values));
  while(g)
  {
    char *value = index(g->data, '\t');

    g_assert(value);
    int lenlabel = (guint) value - (guint) g->data;
    v->label = g_malloc(lenlabel+1);
    strncpy(v->label, g->data, lenlabel);
    *(v->label+lenlabel) = 0;

    value++;
    int lenvalue = strlen(value);
    v->value = g_malloc(lenvalue+1);
    strncpy(v->value, value, lenvalue+1);

    g_printerr(_(" label: '%s' value: '%s'\n"), v->label, v->value);
    v++;
    g = g->next;
  }
  v->label = NULL;
  v->value = NULL;
  return v0;
}


void my_free_values_array(Values **v)
{
  g_return_if_fail(v);
  if(*v == gen_values) return;
  if(*v == xr_values) return;
  if(*v == pos_values) return;
  if(*v == num_values) return;
  if(*v == domain_values) return;
  Values *i = *v;
  while(i && i->value)
  {
    g_free(i->label);
    g_free(i->value);
    i++;
  }
  g_free(v);
  *v = NULL;
}


/// Return the value of the Values array that corresponds to index
/** @retval NULL out of bounds
 */
const gchar *index2value(const Values *values, const int index)
{
  g_return_val_if_fail(values, NULL);
  int i = 0;
  // use 'while' even though we can just index into values[],
  // because we don't know the array length
  while(values && i<index && values->value) { values++; i++; }
  if(values && values->value) return values->value;
  return NULL;
}


/// Return the index in the Values array that corresponds to value
/** @retval -1 unknown value
 */
int value2index(const Values *values, const gchar *value)
{
  g_return_val_if_fail(values, -1);
  if(!value) return 0;

  int i = 0;
  while(values && values->value)
  {
    if(!strcmp(value, values->value)) return i;
    values++;
    i++;
  }
  return -1;
}


// typology for cross references
const Values xr_values_default[] = {
  { N_("Undetermined"), "" },
  { N_("Antonym"), "ant" },
  { N_("Hypernym"), "hyper" },
  { N_("Hyponym"), "hypo" },
  { N_("Synonym"), "syn" },
  { N_("Derived from"), "der" },
  { NULL }
};

const Values pos_values_default[] = {
  { N_("None"), "" },
  { N_("_Noun"), "n" },
  { N_("Verb"), "v" },
  { N_("Transitive Verb"), "vt" },
  { N_("Intransitive Verb"), "vi" },
  { N_("Transitive and intransitive Verb"), "vti" },
  { N_("Adverb"), "adv" },
  { N_("_Adjective"), "adj" },
  { N_("Conjunction"), "conj" },
  { N_("_Preposition"), "prep" },
  { N_("_Interjection"), "interj" },
  { N_("Pronoun"), "pron" },
  { N_("Article"), "art" },
  { N_("Numeral"), "num" },
  { N_("Imitative"), "imit" },
  { N_("Abbreviation"), "abbr" },
  { N_("Phrase"), "phra" },
  { NULL }
};

const Values gen_values_default[] = {
  { N_("None"), "" },
  { N_("Masculine"), "m" },
  { N_("_Feminine"), "f" },
  { N_("Neuter"), "n" },
  { N_("Common"), "i" },
  { N_("Masc. & Fem."), "mf" },
  { N_("Masc., Fem. & Neut."), "mfn" },
  { NULL }
};

const Values num_values_default[] = {
  { N_("None"), "" },
  { N_("_Singular"), "sg" },
  { N_("Dual"), "du" },
  { N_("Plural"), "pl" },
  { NULL }
};

// TEI 12.3.5.2 Usage Information and Other Labels
// Encoded as <usg type="dom">agr</usg>
// in German: "Sachgebiete"
const Values domain_values_default[] = {
  { N_("_None"), "" },

  // taken from fdicts.com
  { N_("_Agriculture"), "agr" },
  { N_("Astronomy"), "astr" },
  { N_("Automobile"), "aut" },
  { N_("_Biology"), "bio" },
  { N_("B_otany"), "bot" },
  { N_("_Chemistry"), "chem" },
  { N_("_Electrotechnics"), "el" },
  { N_("_Finance"), "fin" },
  { N_("_Geography"), "geo" },
  { N_("Geolog_y"), "geol" },
  { N_("Grammar"), "gram" },
  { N_("_History"), "hist" },
  { N_("_Information Technology"), "it" },
  { N_("_Law"), "law" },
  { N_("_Mathematics"), "math" },
  { N_("Me_dicine"), "med" },
  { N_("Military"), "mil" },
  { N_("M_usic"), "mus" },
  { N_("Myth_ology"), "myt" },
  { N_("_Physics"), "phy" },
  { N_("Politics"), "pol" },
  { N_("_Religion"), "rel" },
  { N_("_Sexual"), "sex" },
  { N_("Sport"), "sport" },
  { N_("_Technology"), "tech" },
  { NULL }
};

// Encoded as <usg type="reg">official</usg>
// these are a bit arbitrary
const Values register_values_default[] = {
  { N_("_None"), "" },
  // the word is used in official communication
  { N_("_Official"), "official" },
  // same as official, maybe a bit less, suggested by TEI 12.3.5.2
  { N_("_Formal"), "formal" },
  // the word is used to communicate with small children and by them
  { N_("Ch_ildren Speech"), "chil" },
  // the word is used in informal context, like at home
  { N_("_Colloquial"), "col" },
  // the word is used by certain groups of society only,
  // suggested by TEI 12.3.5.2
  { N_("_Slang"), "slang" },
  // ithe word is used by uneducated people
  { N_("_Vulgar"), "vulg" },
  // the word should not be used?, suggested by TEI 12.3.5.2
  { N_("_Taboo"), "taboo" },
  // the word is used mainly in ironic remarks?,
  // suggested by TEI 12.3.5.2
  { N_("_Ironic"), "ironic" },
  // the word is used mainly in funny context eg. jokes?,
  // suggested by TEI 12.3.5.2
  { N_("_Facetious"), "facetious" },
  { NULL }
};

