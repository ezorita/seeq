/*
** Copyright 2015 Eduard Valera Zorita.
**
** File authors:
**  Eduard Valera Zorita (eduardvalera@gmail.com)
**
** License: 
**  This program is free software: you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation, either version 3 of the License, or
**  (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program.  If not, see <http://www.gnu.org/licenses/>.
**
*/

#include <Python.h>
#include "structmember.h"
#include "libseeq.h"
#include <string.h>

/** Declarations **/

static PyObject * SeeqException;
static PyObject * LibSeeqException;

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
   int        options;
} SeeqObject;

// SeeqIter struct.
typedef struct {
   PyObject_HEAD
   SeeqObject * sqObj;
   PyObject   * strObj;
   const char * string;
   int          match_iter;
   int          last;
} SeeqIter;


/** SeeqIter class **/

static void
SeeqIter_dealloc
(
 SeeqIter * self
)
{
   Py_XDECREF(self->sqObj);
   Py_XDECREF(self->strObj);
   PyObject_Del(self);
}

static SeeqIter *
SeeqIter_new
(
 SeeqObject * sqObj,
 PyObject   * strObj,
 int          match_iter
)
{
   if (sqObj->sq == NULL) {
      PyErr_SetString(SeeqException, "NULL reference to DFA pointer");
      return NULL;
   }
   if (!PyUnicode_Check(strObj)) return NULL;

   SeeqIter * self;
   // TYPE* PyObject_New(TYPE, PyTypeObject *type)
   // Allocate a new Python object using the C structure type TYPE and the Python type object type.
   // [...] the object’s reference count will be one. (So no need to Py_INCREF(self))
   self = PyObject_New(SeeqIter, &SeeqIterType);
   if (self == NULL) {
      return NULL;
   }

   const char * string = PyUnicode_AsUTF8(strObj);
   if (string == NULL) return NULL;

   seeq_t * sq = sqObj->sq;
   if (seeqStringMatch(string, sq, SQ_ALL | sqObj->options) == -1) {
      PyErr_SetString(LibSeeqException, seeqPrintError());
      return NULL;
   }

   Py_INCREF(sqObj);
   Py_INCREF(strObj);
   self->sqObj = sqObj;
   self->strObj = strObj;
   self->string = string;
   self->match_iter = match_iter;
   self->last = 0;

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
   SeeqIter * self  = (SeeqIter *) s;
   seeq_t   * sq    = self->sqObj->sq;
   match_t  * match = seeqMatchIter(sq);

   // StopIteration exception

   if (match) {
      int start = match->start;
      int end   = match->end;
      int last  = self->last;
      self->last = end;
      
      // Return match.
      if (self->match_iter == 1) {
         int toklen = end - start;
         char * new_string = malloc(toklen + 1);
         if (new_string == NULL) return PyErr_NoMemory();
         memcpy(new_string, self->string + start, toklen);
         new_string[toklen] = 0;
         PyObject * new_strObj = PyUnicode_FromString(new_string);
         free(new_string);
         if (new_strObj == NULL) return NULL;
         return new_strObj;
      }
      // Return prefix.
      else {
         int toklen = start - last;
         if (toklen > 0) {
            char * new_string = malloc(toklen + 1);
            if (new_string == NULL) return PyErr_NoMemory();
            memcpy(new_string, self->string + last, toklen);
            new_string[toklen] = 0;
            PyObject * new_strObj = PyUnicode_FromString(new_string);
            free(new_string);
            if (new_strObj == NULL) return NULL;
            return new_strObj;
         }
         // It may happen that we have a zero-length prefix. Iterate again.
         else return SeeqIter_iternext((PyObject *)self);
      }
   }
   // No matches were found.
   else {
      if (self->match_iter == 1) {
         // If return matches only, stop iteration.
         PyErr_SetNone(PyExc_StopIteration);
         return NULL;
      } else {
         // Return last suffix (if any).
         int toklen = strlen(self->string) - self->last;
         if (toklen > 0) {
            char * new_string = malloc(toklen + 1);
            if (new_string == NULL) return PyErr_NoMemory();
            memcpy(new_string, self->string + self->last, toklen);
            new_string[toklen] = 0;
            PyObject * new_strObj = PyUnicode_FromString(new_string);
            free(new_string);
            if (new_strObj == NULL) return NULL;
            else return new_strObj;
         } else {
            // There was no suffix, stop iteration.
            PyErr_SetNone(PyExc_StopIteration);
            return NULL;
         }
      }
   }
}

// Attributes and methods

static PyMemberDef SeeqIter_members[] = {
    {"string", T_OBJECT_EX, offsetof(SeeqIter, strObj), READONLY, "matched string"},
    {NULL}  /* Sentinel */
};

static PyMethodDef SeeqIter_methods[] = {
   {NULL, NULL, 0, NULL}        /* Sentinel */
};

static PyTypeObject SeeqIterType = {
    {PyObject_HEAD_INIT(&PyType_Type)},
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
    PyType_GenericNew,         /* tp_new */
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
   PyObject_Del(self);
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
      PyErr_SetString(SeeqException, "Empty string");
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
 int         pos,
 match_t   * match
)
// This functions add a new match to the SeeqMatch list of matches.
{
   size_t slen = strlen(self->string);
   size_t start = match->start;
   size_t end   = match->end;
   size_t dist  = match->dist;

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

   int maxtokens = 2*nmatches + 1;
   char ** tokens  = malloc(maxtokens * sizeof(char *));
   if (tokens == NULL) return PyErr_NoMemory();

   int ntokens = 0;
   int tstart = 0;
   for (int i = 0; i < nmatches; i++) {
      PyObject * tuple = PyList_GetItem(self->matches, i);
      if (!PyTuple_Check(tuple)) {
         free(tokens);
         return NULL;
      }
      long mstart = PyLong_AsLong(PyTuple_GetItem(tuple, 0));
      long mend   = PyLong_AsLong(PyTuple_GetItem(tuple, 1));

      // Prefix (can be an empty string).
      if (mstart - tstart >= 0) {
         int tokenlen = mstart - tstart;
         char * token = malloc(tokenlen + 1);
         if (token == NULL) {
            free(tokens);
            return PyErr_NoMemory();
         }
         memcpy(token, string + tstart, tokenlen);
         token[tokenlen] = 0;
         tokens[ntokens++] = token;
      }

      // Match.
      if (mend - mstart > 0) {
         int tokenlen = mend - mstart;
         char * token = malloc(tokenlen + 1);
         if (token == NULL) {
            free(tokens);
            return PyErr_NoMemory();
         }
         memcpy(token, string + mstart, tokenlen);
         token[tokenlen] = 0;
         tokens[ntokens++] = token;
      }
      tstart = mend;
   }

   // String suffix.
   int slen = strlen(string);
   if (slen - tstart >= 0) {
         int tokenlen = slen - tstart;
         char * token = malloc(tokenlen + 1);
         if (token == NULL) {
            free(tokens);
            return PyErr_NoMemory();
         }
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
   if (tokens == NULL) return PyErr_NoMemory();

   int ntokens = 0;
   int tstart = 0;
   for (int i = 0; i < nmatches; i++) {
      PyObject * tuple = PyList_GetItem(self->matches, i);
      if (!PyTuple_Check(tuple)) {
         free(tokens);
         return NULL;
      }
      long mstart = PyLong_AsLong(PyTuple_GetItem(tuple, 0));
      long mend   = PyLong_AsLong(PyTuple_GetItem(tuple, 1));

      // Prefix.
      if (mstart - tstart > 0) {
         int tokenlen = mstart - tstart;
         char * token = malloc(tokenlen + 1);
         if (token == NULL) {
            free(tokens);
            return PyErr_NoMemory();
         }
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
         if (token == NULL) {
            free(tokens);
            return PyErr_NoMemory();
         }
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
   if (tokens == NULL) return PyErr_NoMemory();

   int ntokens = 0;
   for (int i = 0; i < nmatches; i++) {
      PyObject * tuple = PyList_GetItem(self->matches, i);
      if (!PyTuple_Check(tuple)) {
         free(tokens);
         return NULL;
      }
      long mstart = PyLong_AsLong(PyTuple_GetItem(tuple, 0));
      long mend   = PyLong_AsLong(PyTuple_GetItem(tuple, 1));

      // Match.
      if (mend - mstart > 0) {
         int tokenlen = mend - mstart;
         char * token = malloc(tokenlen + 1);
         if (token == NULL) {
            free(tokens);
            return PyErr_NoMemory();
         }
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
     "List containing match information tuples: (start, end, distance)"},
    {"string", T_OBJECT_EX, offsetof(SeeqMatch, stringObj), READONLY,
     "matched string"},
    {NULL}  /* Sentinel */
};

static PyMethodDef SeeqMatch_methods[] = {
   {"tokenize", (PyCFunction)SeeqMatch_tokenize, METH_VARARGS, "tokenize()\nTokenizes the matched string at the match start and end sites.\
\nReturns: Tuple containing String objects."},
   {"split", (PyCFunction)SeeqMatch_split, METH_VARARGS, "split()\nSplits the string at the matching sites (matches are not included).\
\nReturns: Tuple containing String objects."},
   {"matches", (PyCFunction)SeeqMatch_matches, METH_VARARGS, "split()\nReturns a tuple with the matches.\
\nReturns: Tuple containing String objects."},
   {NULL, NULL, 0, NULL}        /* Sentinel */
};

static PyTypeObject SeeqMatchType = {
    {PyObject_HEAD_INIT(&PyType_Type)},
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
    PyType_GenericNew,         /* tp_new */
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
   PyObject_Del(self);
}


static SeeqObject *
SeeqObject_new
(
 int          mismatches,
 const char * pattern,
 seeq_t     * sq,
 int          match_options
)
// This is a C function only, it will not be passed as a class method.
// So SeeqObject instances will be only created in C from 'seeq.compile'.

{
   // Check errors.
   if (pattern == NULL || strlen(pattern) < 1) {
      PyErr_SetString(SeeqException, "Empty pattern");
      return NULL;
   }
   if (mismatches < 0) {
      PyErr_SetString(SeeqException, "Mismatches must be a non-negative integer");
      return NULL;
   }
   if (sq == NULL) {
      PyErr_SetString(SeeqException, "NULL reference to DFA pointer");
      return NULL;
   }

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
   self->options = match_options;

   return self;
}

static PyObject *
SeeqObject_matchSuffix
(
 SeeqObject * self,
 PyObject   * args
)
// SeeqObject_match will be a method of the SeeqObject class.
// This function will call the string matching function and return a
// SeeqMatch object instance or Py_None if nothing was found.
{
   PyObject   * include_match = NULL;
   const char * string;

   if (!PyArg_ParseTuple(args,"s|O:match", &string, &include_match))
      return NULL;

   // Read include match option. (Default = True).
   int inc_match;
   if (include_match == NULL) inc_match = 1;
   else {
      inc_match = PyObject_IsTrue(include_match);
      if (inc_match == -1)
         return NULL;
   }

   int rval = seeqStringMatch(string, self->sq, SQ_BEST | self->options);

   // Error.
   if (rval < 0) {
      PyErr_SetString(LibSeeqException, seeqPrintError());
      return NULL;
   }
   // Pattern not found (return Py_None).
   else if (rval == 0) {
      Py_INCREF(Py_None);
      return Py_None;
   }
   // Pattern found (return SeeqMatch instance).
   else {
      char * dup = strdup(string);
      char * suffix;
      if (inc_match) {
         suffix = dup + self->sq->match[0].start; 
      } else {
         suffix = dup + self->sq->match[0].end;
      }
      PyObject * retObj = PyUnicode_FromString(suffix);
      free(dup);
      return retObj;
   }
}

static PyObject *
SeeqObject_matchPrefix
(
 SeeqObject * self,
 PyObject   * args
)
// SeeqObject_match will be a method of the SeeqObject class.
// This function will call the string matching function and return a
// SeeqMatch object instance or Py_None if nothing was found.
{
   PyObject   * include_match = NULL;
   const char * string;

   if (!PyArg_ParseTuple(args,"s|O:match", &string, &include_match))
      return NULL;
   int inc_match;
   if (include_match == NULL) inc_match = 1;
   else {
      inc_match = PyObject_IsTrue(include_match);
      if (inc_match == -1)
         return NULL;
   }

   int rval = seeqStringMatch(string, self->sq, SQ_BEST | self->options);

   // Error.
   if (rval < 0) {
      PyErr_SetString(LibSeeqException, seeqPrintError());
      return NULL;
   }
   // Pattern not found (return Py_None).
   else if (rval == 0) {
      Py_INCREF(Py_None);
      return Py_None;
   }
   // Pattern found (return SeeqMatch instance).
   else {
      char * prefix = strdup(string);
      if (inc_match) {
         prefix[self->sq->match[0].end] = 0;
      } else {
         prefix[self->sq->match[0].start] = 0;
      }
      PyObject * retObj = PyUnicode_FromString(prefix);
      free(prefix);
      return retObj;
   }
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
   if (!PyUnicode_Check(stringObj))
      return NULL;

   string = PyUnicode_AsUTF8(stringObj);

   int rval = seeqStringMatch(string, self->sq, MATCH_OPTIONS | self->options);

   // Error.
   if (rval < 0) {
      PyErr_SetString(LibSeeqException, seeqPrintError());
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
      SeeqMatch * match = SeeqMatch_new(stringObj, string, rval);
      if (match == NULL) {
         return NULL;
      }
      
      seeq_t * sq = self->sq;
      // Add the matches to the match list.
      match_t * match_it;
      int pos = 0;
      while ((match_it = seeqMatchIter(sq)) != NULL) {
         if (!SeeqMatch_add(match, pos++, match_it)) {
            Py_DECREF(match);
            return NULL;
         }
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
SeeqObject_match
(
 SeeqObject * self,
 PyObject   * args
)
{
   return SeeqObject_seeqmatch(self, args, SQ_FIRST);
}

static PyObject *
SeeqObject_matchBest
(
 SeeqObject * self,
 PyObject   * args
)
{
   return SeeqObject_seeqmatch(self, args, SQ_BEST);
}


static PyObject *
SeeqObject_matchAll
(
 SeeqObject * self,
 PyObject   * args
)
{
   return SeeqObject_seeqmatch(self, args, SQ_ALL);
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
   if (!PyUnicode_Check(stringObj))
      return NULL;

   // Return a seeq Iterator with match_iter = 1.
   return (PyObject *)SeeqIter_new(self, stringObj, 1);
}

// FIXME: The function is buggy and should be fixed before being used.

// static PyObject *
// SeeqObject_splitIter
// (
//  SeeqObject * self,
//  PyObject   * args
// )
// // SeeqObject_splitIter will be a method of the SeeqObject class.
// // This function (...)
// {
//    PyObject   * stringObj;      

//    if (!PyArg_ParseTuple(args,"O:splitIter", &stringObj))
//       return NULL;
//    if (!PyUnicode_Check(stringObj))
//       return NULL;

//    // Return a seeq Iterator with match_iter = 0.
//    return (PyObject *)SeeqIter_new(self, stringObj, 0);
// }

// Attributes and methods

static PyMemberDef SeeqObject_members[] = {
    {"pattern", T_OBJECT_EX, offsetof(SeeqObject, pattern), READONLY,
     "matching pattern"},
    {"mismatches", T_OBJECT_EX, offsetof(SeeqObject, mismatches), READONLY,
     "maximum matching distance"},
    {NULL}  /* Sentinel */
};

static PyMethodDef SeeqObject_methods[] = {
   {"match", (PyCFunction)SeeqObject_match, METH_VARARGS,
    "match(String)\nReturns the first match found in the string or None if no matches are found.\
\nReturns: SeeqMatch instance."
   },
   {"matchBest", (PyCFunction)SeeqObject_matchBest, METH_VARARGS,
    "matchBest(String)\nReturns the best match found in the string or None if no matches are found.\
\nReturns: SeeqMatch instance."
   },
   {"matchAll", (PyCFunction)SeeqObject_matchAll, METH_VARARGS,
    "matchAll(String)\nReturns all the matches found in the string or None if no matches are found.\
\nReturns: SeeqMatch instance."
   },
   {"matchIter", (PyCFunction)SeeqObject_matchIter, METH_VARARGS,
    "matchIter(String)\nIteratively returns the matching parts of the string.\nReturns: String iterator."
   },
   // {"splitIter", (PyCFunction)SeeqObject_splitIter, METH_VARARGS,
   //  "splitIter(String)\nIteratively returns the non-matching parts of the string\nReturns: String iterator."
   // },
   {"matchPrefix", (PyCFunction)SeeqObject_matchPrefix, METH_VARARGS,
    "trimPrefix(String,[match=True])\nReturns the prefix before the best match. The match is not \
included in the string if the second argument is set to 'False'.\nReturns: String."
   },
   {"matchSuffix", (PyCFunction)SeeqObject_matchSuffix, METH_VARARGS,
    "trimSuffix(String,[match=True])\nReturns the suffix after the best match. The match is not \
included in the string if the second argument is set to 'False'.\nReturns: String."
   },
   {NULL, NULL, 0, NULL}        /* Sentinel */
};

static PyTypeObject SeeqObjectType = {
   {PyObject_HEAD_INIT(&PyType_Type)},
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
    PyType_GenericNew,         /* tp_new */
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
   int mode = 0;
   size_t memory = 0;
   if (!PyArg_ParseTuple(args,"si|ik:match", &pattern, &mismatches, &mode, &memory))
      return NULL;

   // Convert memory in MB.
   memory *= 1024*1024;

   // Parse options.
   int match_options;
   if (mode == 0) match_options = SQ_CONVERT;
   else match_options = SQ_IGNORE;

   // Generate seeq_t object.
   seeq_t * sq = seeqNew(pattern, mismatches, memory);
   if (sq == NULL) {
      PyErr_SetString(LibSeeqException, seeqPrintError());
      return NULL;
   }

   PyObject * newObj = (PyObject *) SeeqObject_new(mismatches, pattern, sq, match_options);
   if (newObj == NULL) {
      free(sq);
      return NULL;
   } else return newObj;
}


static PyMethodDef SeeqMethods[] = {
   {"compile",  seeq_compile, METH_VARARGS, "compile(pattern, distance, [memory=0])\nCompile a seeq-match object. \
Pattern is a String with the DNA/RNA pattern to be matched. Distance is the allowed Levenshtein distance. \
Memory is an optional argument that indicates the memory limit of the DFA in MB (set to 0 for unlimited memory).\
\nReturns: SeeqObject instance."},
   {NULL, NULL, 0, NULL}        /* Sentinel */
};

PyMODINIT_FUNC
PyInit_seeq
(void)
{
   SeeqObjectType.tp_new = PyType_GenericNew;
   if (PyType_Ready(&SeeqObjectType) < 0)
      return NULL;
   SeeqMatchType.tp_new = PyType_GenericNew;
   if (PyType_Ready(&SeeqMatchType) < 0)
      return NULL; 
   SeeqIterType.tp_new = PyType_GenericNew;
   if (PyType_Ready(&SeeqIterType) < 0)
      return NULL; 

   // PyObject * m = Py_InitModule3("seeq", SeeqMethods, "seeq library for Python.");

   static struct PyModuleDef moduledef = {
      PyModuleDef_HEAD_INIT,
      "seeq",                      /* m_name */
      "seeq library for Python 3.",  /* m_doc */
      -1,                          /* m_size */
      SeeqMethods,                 /* m_methods */
      NULL,                        /* m_reload */
      NULL,                        /* m_traverse */
      NULL,                        /* m_clear */
      NULL,                        /* m_free */
   };

   PyObject * m = PyModule_Create2(&moduledef, PYTHON_API_VERSION);
   if (m == NULL)
      return NULL;

   // Add Seeq Exception objects.
   SeeqException = PyErr_NewException("seeq.exception", NULL, NULL);
   Py_INCREF(SeeqException);
   PyModule_AddObject(m, "exception", SeeqException);

   LibSeeqException = PyErr_NewException("libseeq.exception", NULL, NULL);
   Py_INCREF(LibSeeqException);
   PyModule_AddObject(m, "clibexception", LibSeeqException);

   return m;

}
