#include <Python.h>
#include "structmember.h"
#include "libseeq.h"


// Seeq module
                                                                                                                                                                                                             
static PyMethodDef SeeqMethods[] = {
   {"compile",  seeq_compile, METH_VARARGS, "Compile a seeq-match object."},
   {NULL, NULL, 0, NULL}        /* Sentinel */
};

PyMODINIT_FUNC
initseeq
(void)
{
   (void) Py_InitModule("seeq", SeeqMethods);
}

static PyObject *
seeq_compile
(
 PyObject * self,
 PyObject * args
)
{

}


/** SeeqMatch class **/

static PyTypeObject SeeqMatch_Type;

typedef struct {
   PyObject_HEAD
   // 'matches' will be a PyList containing tuples with the 
   // start, end and distance values for each of the matches.
   PyObject * matches;
   // Will keep a reference to the string to implement
   // class methods to format the match.
   PyObject   * stringObj;
   const char * string;
} SeeqMatch;

static void
SeeqMatch_dealloc
(
 SeeqMatch * self
)
{
   Py_XDECREF(matches);
   Py_XDECREF(stringObj);
   self->ob_type->tp_free((PyObject*)self);
}

static SeeqMatch *
SeeqMatch_new
(
 PyObject   * stringObj,
 const char * string,
 Py_ssize_t   nmatches
)
// This is a C function only, it will not be passed as a class method.
// So SeeqMatch instances will be only created in C from 'SeeqObject.match'.
{
   if (string == NULL) {
      Py_DECREF(stringObj);
      return NULL;
   }
   int slen = strlen(string);
   // dist is set to -1 when the match was not found.
   if (dist < 0) {
      Py_DECREF(stringObj);
      Py_INCREF(Py_None);
      return Py_None;
   }
   
   SeeqMatch * self;
   // TYPE* PyObject_New(TYPE, PyTypeObject *type)
   // Allocate a new Python object using the C structure type TYPE and the Python type object type.
   // [...] the object’s reference count will be one. (So no need to Py_INCREF(self))
   self = PyObject_New(SeeqMatch, &SeeqMatch_Type);
   if (self == NULL) {
      Py_DECREF(stringObj);
      return NULL;
   }

   // Set attributes
   // Create new list of tuples.
   self->matches = PyList_New(nmatches);
   if (self->matches == NULL) {
      Py_DECREF(self);
      Py_DECREF(stringObj);
      return NULL;
   }

   self->stringObj = stringObj;
   self->string = string;

   // return the SeeqMatch instance.
   return self;
}

static int
SeeqMatch_add
(
 SeeqMatch * self,
 int pos,
 int start,
 int end,
 int dist
)
// This functions add a new match to the SeeqMatch list of matches.
{
   if (start < 0 || start > slen || end < 0 || end > slen)
      return 0;
   if (distance < 0)
      return 1;
   
   // Create tuple and insert it into the matches list.
   PyObject * tuple = Py_BuildValue("(iii)", start, end, dist);
   if (tuple == NULL) return 0;

   // Append the tuple to the SeeqMatch match list.
   int retval = PyList_SetItem(self->matches, pos, tuple);
   return (retval ? 1 : 0);
}   

static PyObject *
SeeqMatch_tokenize
(
 SeeqObject * self,
 PyObject   * args
)
// This function tokenizes a string using the compiled pattern.
// The non-matching fragments between matches will be returned
// as a list of independent strings.
{
   
}

// Attributes and methods

static PyMemberDef SeeqMatch_members[] = {
    {"matches", T_OBJECT_EX, offsetof(Noddy, matches), 0,
     "match list"},
    {"string", T_OBJECT_EX, offsetof(Noddy, stringObj), 0,
     "matched string"},
    {NULL}  /* Sentinel */
};

static PyMethodDef SeeqMatch_methods[] = {
    {"tokenize", SeeqMatch_tokenize, METH_VARARGS,
     "Tokenizes the matched string, each token contains one match"
    },
    {NULL}  /* Sentinel */
};

static PyTypeObject NoddyType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "seeq.SeeqMatch",          /*tp_name*/
    sizeof(SeeqMatch),         /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)SeeqMatch_dealloc, /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "SeeqMatch objects",       /* tp_doc */
    0,		               /* tp_traverse */
    0,		               /* tp_clear */
    0,		               /* tp_richcompare */
    0,		               /* tp_weaklistoffset */
    0,		               /* tp_iter */
    0,		               /* tp_iternext */
    SeeqMatch_methods,         /* tp_methods */
    SeeqMatch_members,         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    0,                         /* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
};

/** SeeqObject Class **/

static PyTypeObject SeeqObject_Type;

typedef struct {
   PyObject_HEAD
   PyObject * mismatches;
   PyObject * pattern;
   // Pointer to seeq_t structure.
   // This attribute will be invisible to Python since it will
   // not be declared in the PyMemberDef array.
   seeq_t   * sq;
} SeeqObject;

static void
SeeqObject_dealloc
(
 SeeqObject* self
)
{
   Py_XDECREF(self->mismatches);
   Py_XDECREF(self->pattern);
   self->ob_type->tp_free((PyObject*)self);
}


static SeeqObject *
SeeqObject_new
(
 int      mismatches,
 char   * pattern,
 seeq_t * sq
)
// This is a C function only, it will not be passed as a class method.
// So SeeqObject instances will be only created in C from 'seeq.compile'.

{
   // Check errors.
   if (pattern == NULL || strlen(pattern) < 1) return NULL;
   if (mismatches < 0) return NULL;
   if (sq == NULL) return NULL;

   SeeqObject * self;
   // TYPE* PyObject_New(TYPE, PyTypeObject *type)
   // Allocate a new Python object using the C structure type TYPE and the Python type object type.
   // [...] the object’s reference count will be one. (So no need to Py_INCREF(self))
   self = PyObject_New(SeeqObject, &SeeqObject_Type);
   if (self == NULL) return NULL;
   
   // Set attributes.
   // Pattern
   self->pattern = Py_BuildValue("s", pattern);
   if (self->pattern == NULL) {
      Py_DECREF(self);
      return NULL;
   }

   // Mismatches.
   self->mismatches = Py_BuildValue("i", mismatches);
   if (self->mismatches == NULL) {
      Py_DECREF(self);
      return NULL;
   }

   // seeq_t structure.
   self->seeqt = sq;

   return self;
}

static PyObject *
SeeqObject_match
(
 SeeqObject * self,
 PyObject   * args
)
// SeeqObject_match will be a method of the SeeqObject class.
// This function will call the string matching function and return a
// SeeqMatch object instance or Py_None if nothing was found.
{
   const char * string;

   if (!PyArg_ParseTuple("s:match", &string))
      return NULL;
   
   int rval = seeqStringMatch(string, self->sq, SQ_MATCH);
   // Error.
   if (rval < 0) {
      // TODO: Throw exception.
      return NULL;
   }
   // Pattern not found (return Py_None).
   else if (rval == 0) {
      Py_INCREF(Py_None);
      return Py_None;
   }
   // Pattern found (return SeeqMatch instance).
   else {
      PyObject   * stringObj;      
      if (!PyArg_ParseTuple("O:match", &stringObj))
         return NULL;
      Py_INCREF(stringObj);

      // Create new SeeqMatch instance. (Steals stringObj reference)
      SeeqMatch * match = SeeqMatch_new(string, stringObj, 1);
      if (match == NULL)
         return NULL;
      // Add the match to the match list.
      if (!SeeqMatch_add(match, 0, sq->match.start, sq->match.end, sq->match.distance)) {
         Py_DECREF(match);
         return NULL;
      }
      // Return the SeeqMatch instance.
      // We are giving our reference, so no need to increase the reference count.
      // Py_BuildValue formats:
      // "O" (object) [PyObject *]
      // Pass a Python object untouched (except for its reference count, which is incremented by one).
      // "N" (object) [PyObject *]
      //    Same as O, except it doesn’t increment the reference count on the object.
      return (PyObject *) match;
   }
   
}

static PyObject *
SeeqObject_matchAll
(
 SeeqObject * self,
 PyObject   * args
)
// SeeqObject_matchAll will be a method of the SeeqObject class.
// This function will call the string matching function and return a
// SeeqMatch object instance containing references to all the non-
// overlapping matches of the sequence, or Py_None if nothing was found.
{
   const char * string;

   if (!PyArg_ParseTuple("s:match", &string))
      return NULL;
 
   // Match buffer
   int maxmatches = strlen(string) / self->sq->wlen + 1;
   int startvals  = malloc(maxmatches * sizeof(int));
   int endvals    = malloc(maxmatches * sizeof(int));
   int distvals   = malloc(maxmatches * sizeof(int));
   int nmatches   = 0;

   int offset = 0, rval;
   seeq_t * sq = self->sq;
   while ((rval = seeqStringMatch(string + offset, sq, SQ_MATCH | SQ_FIRST)) > 0) {
      startvals[nmatches] = sq->match.start;
      endvals  [nmatches] = sq->match.end;
      distvals [nmatches] = sq->match.dist;
      offset = sq->match.end;
      nmatches++;
   }
   
   if (rval == -1) {
      // Free buffers and return NULL.
      free(startvals); free(endvals); free(distvals);
      return NULL;
   }

   if (nmatches > 0) {
      PyObject   * stringObj;
      if (!PyArg_ParseTuple("O:match", &stringObj))
         return NULL;
      Py_INCREF(stringObj);

      // Create new SeeqMatch instance. (Steals stringObj reference)
      SeeqMatch * match = SeeqMatch_new(string, stringObj, nmatches);
      if (match == NULL) {
         // Free buffers and return NULL.
         free(startvals); free(endvals); free(distvals);
         return NULL;
      }
      // Add matches to SeeqMatch.
      for (int i = 0; i < nmatches; i++) {
         if (!SeeqMatch_add(match, i, startvals[i], endvals[i], distvals[i])) {
            free(startvals); free(endvals); free(distvals);
            Py_DECREF(match);
            return NULL;
         }
      }
      
      // Free buffers and return.
      free(startvals); free(endvals); free(distvals);
      return (PyObject *) match;
   } else {
      // Free buffers and return.
      free(startvals); free(endvals); free(distvals);
      Py_INCREF(Py_None);
      return Py_None;
   }
}
