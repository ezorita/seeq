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


// SeeqMatch class

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
   Py_DECREF(matches);
   Py_DECREF(stringObj);
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
   if (string == NULL)
      return NULL;
   int slen = strlen(string);
   // dist is set to -1 when the match was not found.
   if (dist < 0) {
      Py_INCREF(Py_None);
      return Py_None;
   }
   
   SeeqMatch * self;
   // TYPE* PyObject_New(TYPE, PyTypeObject *type)
   // Allocate a new Python object using the C structure type TYPE and the Python type object type.
   // [...] the object’s reference count will be one. (So no need to Py_INCREF(self))
   self = PyObject_New(SeeqMatch, &SeeqMatch_Type);
   if (self == NULL) return NULL;

   // Set attributes
   // Create new list of tuples.
   self->matches = PyList_New(nmatches);
   if (self->matches == NULL) {
      Py_DECREF(self);
      return NULL;
   }

   // Let the garbage collector know that we're keeping a reference to the
   // string PyObject.
   Py_INCREF(stringObj);
   self->stringObj = stringObj;
   self->string = string;

   // return the SeeqMatch instance.
   return self;
   
}

static int
SeeqMatch_add
(
 SeeqMatch * self,
 int start,
 int end,
 int dist
)
// This functions add a new match to the SeeqMatch list of matches.
{
   if (start < 0 || start > slen || end < 0 || end > slen)
      return -1;
   if (distance < 0)
      return 0;
   
   // Create tuple and insert it into the matches list.
   PyObject * tuple = PyTuple_New(3);
   if (tuple == NULL) return -1;
   PyObject * startObj = Py_BuildValue("i", start);
   if (startObj == NULL) {
      Py_DECREF(tuple);
      return -1;
   }
   PyObject * endObj = PyBuildValue("i", end);
   if (endObj == NULL) {
      Py_DECREF(tuple); Py_DECREF(startObj);
      return -1;
   }
   PyObject * distObj = PyBuildValue("i", dist);
   if (distObj == NULL) {
      Py_DECREF(tuple); Py_DECREF(startObj); Py_DECREF(endObj);
      return -1;
   }

   // Add objects into the tuple.
   if (!PyTuple_SetItem(tuple, 0, startObj) ||
       !PyTuple_SetItem(tuple, 1, endObj) ||
       !PyTuple_SetItem(tuple, 2, distObj)) {
      Py_DECREF(tuple);
      Py_DECREF(startObj); Py_DECREF(endObj); Py_DECREF(distObj);
      return -1;
   }

   // Append the tuple to the SeeqMatch match list.
   if(!PyList_Append(self->matches, tuple)) {
      Py_DECREF(tuple);
      Py_DECREF(startObj); Py_DECREF(endObj); Py_DECREF(distObj);
      return -1;
   }

   return 0;
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



// SeeqObject Class

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
   Py_DECREF(self->mismatches);
   Py_DECREF(self->pattern);
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
   Py_INCREF(self->pattern);

   // Mismatches.
   self->mismatches = Py_BuildValue("i", mismatches);
   Py_INCREF(self->mismatches);

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
   PyObject   * stringObj;

   if (!PyArg_ParseTuple("s:match", &string))
      return NULL;
   if (!PyArg_ParseTuple("O:match", &stringObj))
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
      // Create new SeeqMatch instance.
      SeeqMatch * match = SeeqMatch_new(string, stringobj, 1);
      if (match == NULL)
         return NULL;
      // Add the match to the match list.
      if (SeeqMatch_add(match, sq->match.start, sq->match.end, sq->match.distance) == -1)
         return NULL;
      // Return the SeeqMatch instance.
      // We are giving our reference, so no need to increase the reference count.
      // Py_BuildValue formats:
      // "O" (object) [PyObject *]
      // Pass a Python object untouched (except for its reference count, which is incremented by one).
      // "N" (object) [PyObject *]
      //    Same as O, except it doesn’t increment the reference count on the object.
      return Py_BuildValue("N", match);
   }
   
}
