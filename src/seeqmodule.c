#include <Python.h>
#include "structmember.h"
#include "libseeq.h"
#include <string.h>

/** Declarations **/

static PyTypeObject SeeqIterType;
static PyTypeObject SeeqMatchType;
static PyTypeObject SeeqObjectType;

// SeeqMatch struct.
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

// SeeqObject struct.
typedef struct {
   PyObject_HEAD
   PyObject * mismatches;
   PyObject * pattern;
   // Pointer to seeq_t structure.
   // This attribute will be invisible to Python since it will
   // not be declared in the PyMemberDef array.
   seeq_t   * sq;
} SeeqObject;

// SeeqIter struct.
typedef struct {
   PyObject_HEAD
   SeeqObject * sqObj;
   PyObject   * stringObj;
   int          pos;
   int          match_iter;
} SeeqIter;



/** SeeqIter class **/

static void
SeeqIter_dealloc
(
 SeeqIter * self
)
{
   Py_XDECREF(self->sqObj);
   Py_XDECREF(self->stringObj);
   self->ob_type->tp_free((PyObject*)self);
}

static SeeqIter *
SeeqIter_new
(
 SeeqObject * sqObj,
 PyObject   * strObj,
 int          match_iter
)
{
   if (sqObj->sq == NULL) return NULL;
   if (!PyString_Check(strObj)) return NULL;

   SeeqIter * self;
   // TYPE* PyObject_New(TYPE, PyTypeObject *type)
   // Allocate a new Python object using the C structure type TYPE and the Python type object type.
   // [...] the object’s reference count will be one. (So no need to Py_INCREF(self))
   self = PyObject_New(SeeqIter, &SeeqIterType);
   if (self == NULL) {
      return NULL;
   }

   Py_INCREF(sqObj);
   Py_INCREF(strObj);
   self->sqObj = sqObj;
   self->stringObj = strObj;
   self->pos = 0;
   self->match_iter = match_iter;

   return self;
}

static PyObject *
SeeqIter_iter
(
 PyObject * self
)
{
   Py_INCREF(self);
   return self;
}

static PyObject *
SeeqIter_iternext
(
 PyObject * s
)
{
   SeeqIter * self = (SeeqIter *) s;

   const char * string = PyString_AsString(self->stringObj);
   if (string == NULL) return NULL;
   int slen = strlen(string);

   // StopIteration exception
   if (self->pos >= slen) {
      PyErr_SetNone(PyExc_StopIteration);
      return NULL;
   }

   seeq_t * sq = self->sqObj->sq;
   int rval  = seeqStringMatch(string + self->pos, sq, SQ_MATCH | SQ_FIRST);
   if (rval > 0) {
      int pos   = self->pos;
      int start = self->pos + sq->match.start;
      int end   = self->pos + sq->match.end;
      self->pos+= sq->match.end;
      
      // Return match.
      if (self->match_iter == 1) {
         int toklen = end - start;
         char * new_string = malloc(toklen + 1);
         memcpy(new_string, string + start, toklen);
         new_string[toklen] = 0;
         PyObject * new_strObj = PyString_FromString(new_string);
         free(new_string);
         if (new_strObj == NULL) return NULL;
         return new_strObj;
      }
      // Return prefix.
      else {
         int toklen = start - pos;
         if (toklen > 0) {
            char * new_string = malloc(toklen + 1);
            memcpy(new_string, string + pos, toklen);
            new_string[toklen] = 0;
            PyObject * new_strObj = PyString_FromString(new_string);
            free(new_string);
            if (new_strObj == NULL) return NULL;
            return new_strObj;
         }
         // It may happen that we have a zero-length prefix. Iterate again.
         else return SeeqIter_iternext((PyObject *)self);
      }
   }
   // No matches were found.
   else if (rval == 0) {
      if (self->match_iter == 1) {
         // If return matches only, stop iteration.
         PyErr_SetNone(PyExc_StopIteration);
         return NULL;
      } else {
         // Return last suffix (if any).
         int toklen = slen - self->pos;
         if (toklen > 0) {
            char * new_string = malloc(toklen + 1);
            memcpy(new_string, string + self->pos, toklen);
            new_string[toklen] = 0;
            PyObject * new_strObj = PyString_FromString(new_string);
            free(new_string);
            self->pos = slen;
            if (new_strObj == NULL) return NULL;
            else return new_strObj;
         } else {
            // There was no suffix, stop iteration.
            PyErr_SetNone(PyExc_StopIteration);
            return NULL;
         }
      }
   }
   else return NULL;

   return NULL;
}

// Attributes and methods

static PyMemberDef SeeqIter_members[] = {
    {"string", T_OBJECT_EX, offsetof(SeeqIter, stringObj), READONLY, "matched string"},
    {NULL}  /* Sentinel */
};

static PyMethodDef SeeqIter_methods[] = {
   {NULL, NULL, 0, NULL}        /* Sentinel */
};

static PyTypeObject SeeqIterType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "seeq.SeeqIter",           /*tp_name*/
    sizeof(SeeqIter),         /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)SeeqIter_dealloc, /*tp_dealloc*/
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
    "SeeqIter objects",        /* tp_doc */
    0,		               /* tp_traverse */
    0,		               /* tp_clear */
    0,		               /* tp_richcompare */
    0,		               /* tp_weaklistoffset */
    SeeqIter_iter,             /* tp_iter */
    SeeqIter_iternext,         /* tp_iternext */
    SeeqIter_methods,         /* tp_methods */
    SeeqIter_members,         /* tp_members */
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


/** SeeqMatch class **/

static void
SeeqMatch_dealloc
(
 SeeqMatch * self
)
{
   Py_XDECREF(self->matches);
   Py_XDECREF(self->stringObj);
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
   if (string == NULL || strlen(string) < 1) {
      Py_DECREF(stringObj);
      return NULL;
   }

   SeeqMatch * self;
   // TYPE* PyObject_New(TYPE, PyTypeObject *type)
   // Allocate a new Python object using the C structure type TYPE and the Python type object type.
   // [...] the object’s reference count will be one. (So no need to Py_INCREF(self))
   self = PyObject_New(SeeqMatch, &SeeqMatchType);
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
   int slen = strlen(self->string);

   if (start < 0 || start > slen || end < 0 || end > slen)
      return 0;
   if (dist < 0)
      return 1;
   
   // Create tuple and insert it into the matches list.
   PyObject * tuple = Py_BuildValue("(iii)", start, end, dist);
   if (tuple == NULL) {
      return 0;
   }

   // Append the tuple to the SeeqMatch match list.
   int retval = PyList_SetItem(self->matches, pos, tuple);
   if (retval == -1) return 0;
   return 1;
}   

static PyObject *
SeeqMatch_tokenize
(
 SeeqMatch * self,
 PyObject  * args
)
// This function tokenizes a string using the compiled pattern.
// The non-matching fragments between matches will be returned
// as a list of independent strings.
{
   const char * string = self->string;
   Py_ssize_t nmatches = PyList_Size(self->matches);
   if (string == NULL || strlen(string) == 0 || nmatches == 0) {
      Py_INCREF(Py_None);
      return Py_None;
   }

   int     maxtokens = 2*nmatches + 1;
   char ** tokens  = malloc(maxtokens * sizeof(char *));
   if (tokens ==  NULL) return NULL;

   int ntokens = 0;
   int tstart = 0;
   for (int i = 0; i < nmatches; i++) {
      PyObject * tuple = PyList_GetItem(self->matches, i);
      if (!PyTuple_Check(tuple)) {
         free(tokens);
         return NULL;
      }
      int mstart = PyInt_AsLong(PyTuple_GetItem(tuple, 0));
      int mend   = PyInt_AsLong(PyTuple_GetItem(tuple, 1));

      // Prefix.
      if (mstart - tstart > 0) {
         int tokenlen = mstart - tstart;
         char * token = malloc(tokenlen + 1);
         memcpy(token, string + tstart, tokenlen);
         token[tokenlen] = 0;
         tokens[ntokens++] = token;
      }

      // Match.
      if (mend - mstart > 0) {
         int tokenlen = mend - mstart;
         char * token = malloc(tokenlen + 1);
         memcpy(token, string + mstart, tokenlen);
         token[tokenlen] = 0;
         tokens[ntokens++] = token;
      }
      tstart = mend;
   }

   // String suffix.
   int slen = strlen(string);
   if (slen - tstart > 0) {
         int tokenlen = slen - tstart;
         char * token = malloc(tokenlen + 1);
         memcpy(token, string + tstart, tokenlen);
         token[tokenlen] = 0;
         tokens[ntokens++] = token;
   }

   // Add tokens to a tuple.
   PyObject * toktuple = PyTuple_New(ntokens);
   if (toktuple == NULL) {
      for (int i = 0; i < ntokens; i++) free(tokens[i]);
      free(tokens);
      return NULL;
   }
   
   for (int i = 0; i < ntokens; i++) {
      //When memory buffers are passed as parameters to supply data to build objects,
      //as for the s and s# formats, the required data is copied. Buffers provided by
      //the caller are never referenced by the objects created by Py_BuildValue().
      //In other words, if your code invokes malloc() and passes the allocated memory
      //to Py_BuildValue(), your code is responsible for calling free() for that memory
      //once Py_BuildValue() returns.
      PyTuple_SetItem(toktuple, i, Py_BuildValue("s", tokens[i]));
      free(tokens[i]);
   }

   free(tokens);
   return toktuple;
}

static PyObject *
SeeqMatch_split
(
 SeeqMatch * self,
 PyObject  * args
)
// This function tokenizes a string using the compiled pattern.
// The non-matching fragments between matches will be returned
// as a list of independent strings.
{
   const char * string = self->string;
   Py_ssize_t nmatches = PyList_Size(self->matches);
   if (string == NULL || strlen(string) == 0 || nmatches == 0) {
      Py_INCREF(Py_None);
      return Py_None;
   }

   int     maxtokens = 2*nmatches + 1;
   char ** tokens  = malloc(maxtokens * sizeof(char *));
   if (tokens ==  NULL) return NULL;

   int ntokens = 0;
   int tstart = 0;
   for (int i = 0; i < nmatches; i++) {
      PyObject * tuple = PyList_GetItem(self->matches, i);
      if (!PyTuple_Check(tuple)) {
         free(tokens);
         return NULL;
      }
      int mstart = PyInt_AsLong(PyTuple_GetItem(tuple, 0));
      int mend   = PyInt_AsLong(PyTuple_GetItem(tuple, 1));

      // Prefix.
      if (mstart - tstart > 0) {
         int tokenlen = mstart - tstart;
         char * token = malloc(tokenlen + 1);
         memcpy(token, string + tstart, tokenlen);
         token[tokenlen] = 0;
         tokens[ntokens++] = token;
      }

      tstart = mend;
   }

   // String suffix.
   int slen = strlen(string);
   if (slen - tstart > 0) {
         int tokenlen = slen - tstart;
         char * token = malloc(tokenlen + 1);
         memcpy(token, string + tstart, tokenlen);
         token[tokenlen] = 0;
         tokens[ntokens++] = token;
   }

   // Add tokens to a tuple.
   PyObject * toktuple = PyTuple_New(ntokens);
   if (toktuple == NULL) {
      for (int i = 0; i < ntokens; i++) free(tokens[i]);
      free(tokens);
      return NULL;
   }
   
   for (int i = 0; i < ntokens; i++) {
      //When memory buffers are passed as parameters to supply data to build objects,
      //as for the s and s# formats, the required data is copied. Buffers provided by
      //the caller are never referenced by the objects created by Py_BuildValue().
      //In other words, if your code invokes malloc() and passes the allocated memory
      //to Py_BuildValue(), your code is responsible for calling free() for that memory
      //once Py_BuildValue() returns.
      PyTuple_SetItem(toktuple, i, Py_BuildValue("s", tokens[i]));
      free(tokens[i]);
   }

   free(tokens);
   return toktuple;
}

static PyObject *
SeeqMatch_matches
(
 SeeqMatch * self,
 PyObject  * args
)
// This function tokenizes a string using the compiled pattern.
// The non-matching fragments between matches will be returned
// as a list of independent strings.
{
   const char * string = self->string;
   Py_ssize_t nmatches = PyList_Size(self->matches);
   if (string == NULL || strlen(string) == 0 || nmatches == 0) {
      Py_INCREF(Py_None);
      return Py_None;
   }

   int     maxtokens = 2*nmatches + 1;
   char ** tokens  = malloc(maxtokens * sizeof(char *));
   if (tokens ==  NULL) return NULL;

   int ntokens = 0;
   for (int i = 0; i < nmatches; i++) {
      PyObject * tuple = PyList_GetItem(self->matches, i);
      if (!PyTuple_Check(tuple)) {
         free(tokens);
         return NULL;
      }
      int mstart = PyInt_AsLong(PyTuple_GetItem(tuple, 0));
      int mend   = PyInt_AsLong(PyTuple_GetItem(tuple, 1));

      // Match.
      if (mend - mstart > 0) {
         int tokenlen = mend - mstart;
         char * token = malloc(tokenlen + 1);
         memcpy(token, string + mstart, tokenlen);
         token[tokenlen] = 0;
         tokens[ntokens++] = token;
      }
   }

   // Add tokens to a tuple.
   PyObject * toktuple = PyTuple_New(ntokens);
   if (toktuple == NULL) {
      for (int i = 0; i < ntokens; i++) free(tokens[i]);
      free(tokens);
      return NULL;
   }
   
   for (int i = 0; i < ntokens; i++) {
      //When memory buffers are passed as parameters to supply data to build objects,
      //as for the s and s# formats, the required data is copied. Buffers provided by
      //the caller are never referenced by the objects created by Py_BuildValue().
      //In other words, if your code invokes malloc() and passes the allocated memory
      //to Py_BuildValue(), your code is responsible for calling free() for that memory
      //once Py_BuildValue() returns.
      PyTuple_SetItem(toktuple, i, Py_BuildValue("s", tokens[i]));
      free(tokens[i]);
   }

   free(tokens);
   return toktuple;
}

// Attributes and methods

static PyMemberDef SeeqMatch_members[] = {
    {"matchlist", T_OBJECT_EX, offsetof(SeeqMatch, matches), READONLY,
     "match list"},
    {"string", T_OBJECT_EX, offsetof(SeeqMatch, stringObj), READONLY,
     "matched string"},
    {NULL}  /* Sentinel */
};

static PyMethodDef SeeqMatch_methods[] = {
   {"tokenize", (PyCFunction)SeeqMatch_tokenize, METH_VARARGS, "Tokenizes the matched string"},
   {"split", (PyCFunction)SeeqMatch_split, METH_VARARGS, "Splits the string removing the matches"},
   {"matches", (PyCFunction)SeeqMatch_matches, METH_VARARGS, "Returns a tuple with the matches"},
   {NULL, NULL, 0, NULL}        /* Sentinel */
};

static PyTypeObject SeeqMatchType = {
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

static void
SeeqObject_dealloc
(
 SeeqObject* self
)
{
   Py_XDECREF(self->mismatches);
   Py_XDECREF(self->pattern);
   seeqFree(self->sq);
   self->ob_type->tp_free((PyObject*)self);
}


static SeeqObject *
SeeqObject_new
(
 int      mismatches,
 const char   * pattern,
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
   self = PyObject_New(SeeqObject, &SeeqObjectType);
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
   self->sq = sq;

   return self;
}

static PyObject *
SeeqObject_seeqmatch
(
 SeeqObject * self,
 PyObject   * args,
 int          MATCH_OPTIONS
)
// SeeqObject_match will be a method of the SeeqObject class.
// This function will call the string matching function and return a
// SeeqMatch object instance or Py_None if nothing was found.
{
   PyObject   * stringObj;      
   const char * string;

   if (!PyArg_ParseTuple(args,"O:match", &stringObj))
      return NULL;
   if (!PyString_Check(stringObj))
      return NULL;

   string = PyString_AsString(stringObj);

   int rval = seeqStringMatch(string, self->sq, MATCH_OPTIONS);

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
      // Will keep a copy of the string PyObject.
      Py_INCREF(stringObj);
      // Create new SeeqMatch instance. (Steals stringObj reference)
      SeeqMatch * match = SeeqMatch_new(stringObj, string, 1);
      if (match == NULL) {
         return NULL;
      }
      
      seeq_t * sq = self->sq;
      // Add the match to the match list.
      if (!SeeqMatch_add(match, 0, sq->match.start, sq->match.end, sq->match.dist)) {
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
SeeqObject_find
(
 SeeqObject * self,
 PyObject   * args
)
{
   return SeeqObject_seeqmatch(self, args, SQ_MATCH | SQ_FIRST);
}

static PyObject *
SeeqObject_findBest
(
 SeeqObject * self,
 PyObject   * args
)
{
   return SeeqObject_seeqmatch(self, args, SQ_MATCH | SQ_BEST);
}


static PyObject *
SeeqObject_findAll
(
 SeeqObject * self,
 PyObject   * args
)
// SeeqObject_matchAll will be a method of the SeeqObject class.
// This function will call the string matching function and return a
// SeeqMatch object instance containing references to all the non-
// overlapping matches of the sequence, or Py_None if nothing was found.
{

   PyObject   * stringObj;      
   const char * string;

   if (!PyArg_ParseTuple(args,"O:findAll", &stringObj))
      return NULL;
   if (!PyString_Check(stringObj))
      return NULL;

   string = PyString_AsString(stringObj);

   // Match buffer
   seeq_t * sq = self->sq;
   int   maxmatches = strlen(string) / sq->wlen + 1;
   int * startvals  = malloc(maxmatches * sizeof(int));
   int * endvals    = malloc(maxmatches * sizeof(int));
   int * distvals   = malloc(maxmatches * sizeof(int));
   int   nmatches   = 0;

   int offset = 0, rval;
   while ((rval = seeqStringMatch(string + offset, sq, SQ_MATCH | SQ_FIRST)) > 0) {
      startvals[nmatches] = offset + sq->match.start;
      endvals  [nmatches] = offset + sq->match.end;
      distvals [nmatches] = sq->match.dist;
      offset += sq->match.end;
      nmatches++;
   }
   
   if (rval == -1) {
      // Free buffers and return NULL.
      free(startvals); free(endvals); free(distvals);
      return NULL;
   }

   if (nmatches > 0) {
      // Will keep a copy of the string PyObject.
      Py_INCREF(stringObj);

      // Create new SeeqMatch instance. (Steals stringObj reference)
      SeeqMatch * match = SeeqMatch_new(stringObj, string, nmatches);
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


static PyObject *
SeeqObject_matchIter
(
 SeeqObject * self,
 PyObject   * args
)
// SeeqObject_matchIter will be a method of the SeeqObject class.
// This function (...)
{
   PyObject   * stringObj;      

   if (!PyArg_ParseTuple(args,"O:matchIter", &stringObj))
      return NULL;
   if (!PyString_Check(stringObj))
      return NULL;

   // Return a seeq Iterator with match_iter = 1.
   return (PyObject *)SeeqIter_new(self, stringObj, 1);
}

static PyObject *
SeeqObject_splitIter
(
 SeeqObject * self,
 PyObject   * args
)
// SeeqObject_splitIter will be a method of the SeeqObject class.
// This function (...)
{
   PyObject   * stringObj;      

   if (!PyArg_ParseTuple(args,"O:matchIter", &stringObj))
      return NULL;
   if (!PyString_Check(stringObj))
      return NULL;

   // Return a seeq Iterator with match_iter = 0.
   return (PyObject *)SeeqIter_new(self, stringObj, 0);
}

// Attributes and methods

static PyMemberDef SeeqObject_members[] = {
    {"pattern", T_OBJECT_EX, offsetof(SeeqObject, pattern), READONLY,
     "matching pattern"},
    {"mismatches", T_OBJECT_EX, offsetof(SeeqObject, mismatches), READONLY,
     "maximum matching distance"},
    {NULL}  /* Sentinel */
};

static PyMethodDef SeeqObject_methods[] = {
   {"find", (PyCFunction)SeeqObject_find, METH_VARARGS,
    "Returns the first match found in the string or None if no matches are found"
   },
   {"findBest", (PyCFunction)SeeqObject_findBest, METH_VARARGS,
    "Returns the best match found in the string or None if no matches are found"
   },
   {"findAll", (PyCFunction)SeeqObject_findAll, METH_VARARGS,
    "Returns all the matches found in the string or None if no matches are found"
   },
   {"matchIter", (PyCFunction)SeeqObject_matchIter, METH_VARARGS,
    "Iteratively returns the matching parts of the string"
   },
   {"splitIter", (PyCFunction)SeeqObject_splitIter, METH_VARARGS,
    "Iteratively returns the non-matching parts of the string"
   },
   {NULL, NULL, 0, NULL}        /* Sentinel */
};

static PyTypeObject SeeqObjectType = {
   PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "seeq.SeeqObject",         /*tp_name*/
    sizeof(SeeqObject),        /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)SeeqObject_dealloc, /*tp_dealloc*/
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
    "SeeqObject objects",      /* tp_doc */
    0,		               /* tp_traverse */
    0,		               /* tp_clear */
    0,		               /* tp_richcompare */
    0,		               /* tp_weaklistoffset */
    0,		               /* tp_iter */
    0,		               /* tp_iternext */
    SeeqObject_methods,        /* tp_methods */
    SeeqObject_members,        /* tp_members */
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


/** Seeq Module **/

static PyObject *
seeq_compile
(
 PyObject * self,
 PyObject * args
)
{
   const char * pattern;
   int mismatches;
   if (!PyArg_ParseTuple(args,"si:match", &pattern, &mismatches))
      return NULL;

   if (pattern == NULL) return NULL;
   if (mismatches < 0) return NULL;

   // Generate seeq_t object.
   seeq_t * sq = seeqNew(pattern, mismatches);
   if (sq == NULL) return NULL;

   PyObject * newObj = (PyObject *) SeeqObject_new(mismatches, pattern, sq);

   return newObj;
}


static PyMethodDef SeeqMethods[] = {
   {"compile",  seeq_compile, METH_VARARGS, "Compile a seeq-match object."},
   {NULL, NULL, 0, NULL}        /* Sentinel */
};

PyMODINIT_FUNC
initseeq
(void)
{
   SeeqObjectType.tp_new = PyType_GenericNew;
   if (PyType_Ready(&SeeqObjectType) < 0)
      return;
   SeeqMatchType.tp_new = PyType_GenericNew;
   if (PyType_Ready(&SeeqMatchType) < 0)
      return; 
   SeeqIterType.tp_new = PyType_GenericNew;
   if (PyType_Ready(&SeeqIterType) < 0)
      return; 

   Py_InitModule3("seeq", SeeqMethods, "seeq library for Python.");
}
