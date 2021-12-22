/** @file
 * @brief XML/XPath/XSLT Utility functions
 */

#include "xml.h"
#include <gnome.h>
#include <libxml/xpathInternals.h>

/////////////////////////////////////////////////////////////////////////
// libxslt/XPath extension functions
/////////////////////////////////////////////////////////////////////////

/** This extension function is designed to be used in a sanity test with an
 * XPath expression like this:
 * "//entry[ fd:unbalanced-braces(.//orth | .//tr | .//note | .//def | .//q) ]"
 * Before its use, a namespace prefix like "fd" has to be bound to
 * FREEDICT_EDITOR_NAMESPACE.
 */
static void freedict_xpath_extension_unbalanced_braces(
    xmlXPathParserContextPtr ctxt, const int nargs)
{

  if(nargs != 1)
  {
    xmlXPathSetArityError(ctxt);
    return;
  }

  xmlNodeSetPtr ns = xmlXPathPopNodeSet(ctxt);
  if(xmlXPathCheckError(ctxt) || !ns)
  {
    xmlXPathFreeNodeSet(ns);
    return;
  }

  // function that does the actual parsing
  // returns TRUE if a brace of the string in c does not have
  // a corresponding brace
  gboolean contains_unbalanced_braces(xmlChar *c)
  {
    if(!c) return FALSE;
    char stack[100];
    int stackend = sizeof(stack);

    // returns FALSE on stack full
    gboolean cub_push(const char b)
    {
      if(!stackend)
      {
        g_printerr(G_STRLOC ": Too many open braces");
        return FALSE;
      }
      stack[--stackend] = b;
      return TRUE;
    }

    gchar cub_pop()
    {
      // stack is empty
      if(stackend>=sizeof(stack)) return 'E';

      return stack[stackend++];
    }

    do
    {
      switch(*c)
      {
        case '(': if(!cub_push('(')) return TRUE; break;
        case '[': if(!cub_push('[')) return TRUE; break;
        case '{': if(!cub_push('{')) return TRUE; break;

        case ')': if(cub_pop()!='(') return TRUE;break;
        case ']': if(cub_pop()!='[') return TRUE;break;
        case '}': if(cub_pop()!='{') return TRUE;break;

        // all other characters are skipped
      }
      c++;
    }
    while(*c);

    // braces left open?
    if(cub_pop()!='E') return TRUE;

    // this string is well formed in regard of braces
    return FALSE;
  }

  int result = FALSE;
  int i;
  for(i=0; i < xmlXPathNodeSetGetLength(ns); i++)
  {
    xmlNodePtr n = xmlXPathNodeSetItem(ns, i);
    xmlChar* c = xmlNodeGetContent(n);
    if(!c) continue;
    result = contains_unbalanced_braces(c);
    xmlFree(c);
    if(result) break;
  }

  if(ns) xmlXPathFreeNodeSet(ns);
  xmlXPathReturnBoolean(ctxt, result);
}


/// Call this on application startup. Or not, since it is presently unused...
void register_freedict_xpath_extension_functions(void)
{

//  g_printerr("Registering XPath extension functions in namespace "
//      FREEDICT_EDITOR_NAMESPACE "\n");
/*
  // The following should result in xmlXPathRegisterNs and
  // xmlXPathRegisterFuncNS being called. But since that does not
  // happen apparently, we register our extension function
  // "manually" in find_node_set()
  xsltRegisterExtModuleFunction ((const xmlChar *) "unbalanced-braces",
      (const xmlChar *) FREEDICT_XSLT_NAMESPACE,
      freedict_xpath_extension_unbalanced_braces);
*/
}

/////////////////////////////////////////////////////////////////////////
// General XML/XPath utility functions
/////////////////////////////////////////////////////////////////////////

xmlDocPtr copy_node_to_doc(const xmlNodePtr node)
{
  g_return_val_if_fail(node, NULL);
  xmlDocPtr doc = xmlNewDoc((xmlChar *) XML_DEFAULT_VERSION);
  xmlNodePtr root = xmlDocCopyNode(node, doc, 1);// copies recursively
  xmlDocSetRootElement(doc, root);
  g_assert(doc);
  return doc;
}

// Th error code XPATH_INVALID_CTXT exists only since libxml2 2.6.0, so
// we define it if we use an earlier version
#if LIBXML_VERSION < 20600
#define XPATH_INVALID_CTXT XPATH_EXPR_ERROR
#endif

// XXX report the fillowing fix
// xmlXPatherror takes an XPath parser context, not a plain XPath context,
// so this macro from the libxml2 source is incorrect, causing gcc to give a
// "warning: passing arg 1 of `xmlXPatherror' from incompatible pointer type"
// (since a parser context has no "doc" member, the macro actually is
// for checking a plain context!)
/*#define CHECK_CONTEXT(ctxt)                                             \
    if ((ctxt == NULL) || (ctxt->doc == NULL) ||                        \
        (ctxt->doc->children == NULL)) {                                \
        xmlXPatherror(ctxt, __FILE__, __LINE__, XPATH_INVALID_CTXT);    \
        return(NULL);                                                   \
    }
*/
// therefore I have extended it with another parameter
#define CHECK_CONTEXT(ctxt, pctxt)                                      \
    if ((ctxt == NULL) || (ctxt->doc == NULL) ||                        \
        (ctxt->doc->children == NULL)) {                                \
        xmlXPatherror(pctxt, __FILE__, __LINE__, XPATH_INVALID_CTXT);   \
        return(NULL);                                                   \
    }

extern GMutex *find_nodeset_pcontext_mutex;

/**
 * @arg str the XPath expression
 * @arg ctxt the XPath context
 * @arg pctxt pointer to a XPath Parser context pointer
 *
 * Evaluate the XPath expression in the given context.  The XPath Parser
 * context is saved in pctxt, so that it can be accessed from another thread.
 * Especially the error state is interesting, since it can be used to stop a
 * never ending evaluation.
 *
 * Taken from xpath.c in libxml2-2.6.16.
 *
 * @return the xmlXPathObjectPtr resulting from the evaluation or NULL.
 *         The caller has to free the object.
 */
xmlXPathObjectPtr
my_xmlXPathEvalExpression(const xmlChar *str, xmlXPathContextPtr ctxt, xmlXPathParserContextPtr *pctxt) {
    xmlXPathObjectPtr res, tmp;
    int stack = 0;

    xmlXPathInit();

    // it is nice that gcc gives no warning anymore,
    // but the bad thing is that *pctxt normally is still NULL at this point
    CHECK_CONTEXT(ctxt,*pctxt)

    g_mutex_lock(find_nodeset_pcontext_mutex);
    //g_printerr("Allocating parser context\n");
    *pctxt = xmlXPathNewParserContext(str, ctxt);
    g_mutex_unlock(find_nodeset_pcontext_mutex);

    xmlXPathEvalExpr(*pctxt);

    if (*(*pctxt)->cur != 0) {
        xmlXPatherror(*pctxt, __FILE__, __LINE__, XPATH_EXPR_ERROR);
        res = NULL;
    } else {
        res = valuePop(*pctxt);
    }
    do {
        tmp = valuePop(*pctxt);
        if (tmp != NULL) {
            xmlXPathFreeObject(tmp);
            stack++;
        }
    } while (tmp != NULL);
    if ((stack != 0) && (res != NULL)) {
        xmlGenericError(xmlGenericErrorContext,
                "xmlXPathEvalExpression: %d object left on the stack\n",
                stack);
    }

    g_mutex_lock(find_nodeset_pcontext_mutex);
    xmlXPathFreeParserContext(*pctxt);
    *pctxt = NULL;
    //g_printerr("Freed parser context\n");
    g_mutex_unlock(find_nodeset_pcontext_mutex);

    return(res);
}


/// Evaluate an XPath expression
/**
 * @arg xpath XPath expression to evaluate
 * @doc document over which to evaluate
 * @arg pctxt can be NULL
 * @return list of matching nodes. The caller will have to free it using xmlXPathFreeNodeSet().
 */
xmlNodeSetPtr find_node_set(const char *xpath, const xmlDocPtr doc, xmlXPathParserContextPtr *pctxt)
{
  xmlXPathContextPtr ctxt = xmlXPathNewContext(doc);
  if(!ctxt)
  {
    g_printerr(G_STRLOC ": Failed to allocate XPathContext!\n");
    return NULL;
  }

  if(xmlXPathRegisterNs(ctxt, (xmlChar *) FREEDICT_EDITOR_NAMESPACE_PREFIX, (xmlChar *) FREEDICT_EDITOR_NAMESPACE))
  {
    g_printerr("Warning: Unable to register XSLT-Namespace prefix \"%s\""
	" for URI \"%s\"\n", FREEDICT_EDITOR_NAMESPACE_PREFIX, FREEDICT_EDITOR_NAMESPACE);
  }

  if(xmlXPathRegisterFuncNS(ctxt, (xmlChar *) "unbalanced-braces",
	(xmlChar *) FREEDICT_EDITOR_NAMESPACE, freedict_xpath_extension_unbalanced_braces))
    g_printerr("Warning: Unable to register XPath extension function "
	"\"unbalanced-braces\" for URI \"%s\"\n", FREEDICT_EDITOR_NAMESPACE);

  xmlXPathParserContextPtr pctxt2;
  if(!pctxt) pctxt = &pctxt2;
  xmlXPathObjectPtr xpobj = my_xmlXPathEvalExpression((xmlChar *) xpath, ctxt, pctxt);
  if(!xpobj)
  {
    g_printerr(G_STRLOC ": No XPathObject!\n");
    xmlXPathFreeContext(ctxt);
    return NULL;
  }

  if(!(xpobj->nodesetval))
  {
    g_printerr(G_STRLOC ": No nodeset!\n");
    xmlXPathFreeObject(xpobj);
    xmlXPathFreeContext(ctxt);
    return NULL;
  }

  if(!(xpobj->nodesetval->nodeNr))
  {
    //g_printerr("0 nodes!\n");
    xmlXPathFreeObject(xpobj);
    xmlXPathFreeContext(ctxt);
    return NULL;
  }

  xmlXPathFreeContext(ctxt);

  xmlNodeSetPtr nodes = xmlMalloc(sizeof(xmlNodeSet));
  // XXX copying is slow...
  memcpy(nodes, xpobj->nodesetval, sizeof(xmlNodeSet));

  // I don't understand the naming of this function.  According to the
  // documentation, it frees xpobj, but not its nodelist, if it
  // contained one. So it should be called xmlXPathFreeObjectButNotNodeSetList().
  xmlXPathFreeNodeSetList(xpobj);

  return nodes;
}


xmlNodePtr find_single_node(const char *xpath, const xmlDocPtr doc)
{
  xmlNodeSetPtr nodes = find_node_set(xpath, doc, NULL);
  if(!nodes) return NULL;
  if(nodes->nodeNr>1)
    g_printerr(_("%s: %i matching nodes (only 1 expected). Taking first.\n"),
      G_STRLOC, nodes->nodeNr);

  xmlNodePtr bodyNode = *(nodes->nodeTab);
  xmlXPathFreeNodeSet(nodes);
  return bodyNode;
}


/** Checks whether @a n has only allowed attrbutes and attr contents,
 * as well as only children of type TEXT
 *
 * @arg attrs list of allowed attribute names
 * @arg attr_contents list of allowed attribute contents
 *      (if an entry is NULL, any contetn is allowed)
 */
gboolean has_only_text_children_and_allowed_attrs(const xmlNodePtr n,
    const char **attrs, const char **attr_contents)
{
  g_return_val_if_fail(n, FALSE);

  // elements may have only the mentioned attributes
  if(n->type==XML_ELEMENT_NODE)
  {
    //g_debug("checking elem '%s'...", n->name);
    // if there are attributes
    if(n->properties)
    {
      g_debug("checking element with attrs... ");
      // if there are no attributes allowed
      if(!attrs) return FALSE;
      g_debug("certain attrs are allowed. ");

      // for all attributes of the element
      xmlAttr *nattrs = n->properties;
      while(nattrs)
      {
	g_return_val_if_fail(nattrs->name, FALSE);

        // check whether attribute is in list of allowed attribute names
        gboolean allowed = FALSE;
        const char **attr = attrs;
        const char **attr_content = attr_contents;
	xmlChar *attr_value = xmlNodeGetContent(nattrs->children);
        g_debug("element attr '%s': value='%s'", nattrs->name, attr_value);
        while(*attr)
        {
	  // if allowed node exists
          g_debug("checking allowed attr '%s': attr_content='%s' ", *attr,
	      *attr_content);
	  if(!strcmp((char *) nattrs->name, (char *) *attr) &&
	      (!attr_content ||
	     !strcmp((char *) attr_value, (char *) *attr_content)))
	  { allowed = TRUE; break; }
          attr++;
          if(attr_content) attr_content++;
	}
        g_debug("%i ", allowed);
	xmlFree(attr_value);
	if(!allowed) return FALSE;
	nattrs = nattrs->next;
      }
    }
    //g_debug("elem passed allowed attr check.\n");
  }

  // if we reach here, element has only allowed attributes:
  // check that it has only text children
  xmlNodePtr n2 = n->children;
  while(n2)
  {
    if(!xmlNodeIsText(n2)) return FALSE;
    g_assert(n2->children == NULL);
    n2 = n2->next;
  }
  return TRUE;
}


/// Look for a matching leaf node
/** This function evaluates an XPath expression.  The result should be a single
 * node with only text children and the attributes listed in @a attrs.  If such
 * node exists, it is unlinked and returned.  If there is no such leaf, no
 * unlinking is done and @a can is set to FALSE.
 *
 * @return The unlinked node. It has to be freed by the caller with xmlFreeNode().
 * @retval NULL if no matching leaf node was found
 */
xmlNodePtr unlink_leaf_node_with_attr(const char *xpath,
    const char **attrs, const char **attr_contents,
    const xmlDocPtr doc, gboolean *can)
{

  g_return_val_if_fail(xpath && doc && can, NULL);

  xmlNodePtr n = find_single_node(xpath, doc);
  if(!n) return NULL;

  if(!has_only_text_children_and_allowed_attrs(n, attrs, attr_contents))
  {
    *can = FALSE;
    return NULL;
  }

  xmlUnlinkNode(n);
  return n;
}


xmlNodePtr string2xmlNode(const xmlNodePtr parent, const gchar *before,
    const gchar *name, const gchar *content, const gchar *after)
{
  g_return_val_if_fail(name, NULL);

  xmlNodeAddContent(parent, (xmlChar *) before);
  xmlNodePtr newNode = xmlNewChild(parent, NULL,
      (xmlChar *) name, (xmlChar *) content);
  xmlNodeAddContent(parent, (xmlChar *) after);

  return newNode;
}


/// Join orth elements of an entry with commas
/** @arg n entry node
 * @arg len size of @a *s in bytes
 * @arg s pointer where result will be saved, error string on failure
 * @retval TRUE on success
 * @retval FALSE on error
 */
gboolean entry_orths_to_string(xmlNodePtr n, int len, char *s)
{
  g_return_val_if_fail(n, FALSE);
  g_return_val_if_fail(s, FALSE);
  g_return_val_if_fail(len>0, FALSE);

  xmlDocPtr doc = copy_node_to_doc(n);

  // find the orth children of the current entry
  xmlNodeSetPtr set = find_node_set("/entry/form/orth", doc, NULL);

  if(!set || !set->nodeNr)
  {
    g_strlcpy(s, _("No nodes (form/orth)!"), len);
    if(set) xmlXPathFreeNodeSet(set);
    xmlFreeDoc(doc);
    return FALSE;
  }

  // alloc temporary buffer
  // if glib offered g_utf8_strlcat(), we would not need
  // this buffer
  char *e = (char *) g_malloc(len);
  e[0] = '\0';

  int i;
  xmlNodePtr *n2;
  for(i=0, n2 = set->nodeTab; *n2 && i<set->nodeNr; n2++, i++)
  {
    xmlChar* content = xmlNodeGetContent(*n2);
    int l = strlen(e);
    if(l) g_strlcat(e, ", ", len);
    if(!content) g_strlcat(e, "(null)", len);
    else g_strlcat(e, (gchar *) content, len);
    if(content) xmlFree(content);
  }

  xmlXPathFreeNodeSet(set);

  // copy again, caring for utf8 chars longer than 1 byte
  g_utf8_strncpy(s, e, len/2);

  g_free(e);
  xmlFreeDoc(doc);
  return TRUE;
}

