
/* ========================================================================== */

/*
 *                             P y m c s r c h
 *
 * P y t h o n   i n t e r f a c e   t o   t h e   M C S R C H   p a c k a g e
 */

/* mcsrch is Jorge Nocedal's safeguarded modification of the More and Thuente
 * linesearch ensuring satisfaction of the strong Wolfe conditions.
 */

/* ========================================================================== */

#include "Python.h"                /* Main Python header file */
#include "numpy/arrayobject.h"     /* NumPy header */
#include "printf.h"

/* ========================================================================== */

/*
 *  D e f i n i t i o n   o f   P y m c s r c h   c o n t e x t   o b j e c t
 */

/* ========================================================================== */

static PyTypeObject PymcsrchType;  /* Precise definition appears below */

typedef struct PymcsrchObject {
    PyObject_VAR_HEAD
    int        n;                                      /* Number of variables */
    double     fk;                                 /* Initial objective value */
    double     gk;                              /* Initial objective gradient */
    double     *dk;                                       /* Search direction */
    double     stp;                                     /* Current steplength */
    double     ftol, gtol, xtol;                    /* Convergence tolerances */
    double     stpmin, stpmax;                             /* Current bracket */
    double     *wa;                                             /* Work array */
    int        maxfev;                  /* Max number of function evaluations */
    int        nfev;                /* Current number of function evaluations */
    int        info;                                                  /* flag */
} PymcsrchObject;

/* Support different Fortran compilers */
#ifdef _AIX
#define FUNDERSCORE(a) a
#else
#define FUNDERSCORE(a) a##_
#endif

#define MCSRCH     FUNDERSCORE(mcsrch)
#define MAX(a,b)   (a) > (b) ? (a) : (b)
#define PymcsrchObject_Check(v)  ((v)->ob_type == &PymcsrchType)

/* ========================================================================== */

//DL_EXPORT( void ) init_pymcsrch( void );
//int init_pymcsrch( void );
PyObject *PyInit__pymcsrch(void);
static PymcsrchObject *NewPymcsrchObject( int n, double ftol, double gtol,
                                          double xtol, double stp,
                                          double stpmin, double stpmax,
                                          int maxfev, double *d );
static PyObject *Pymcsrch_Init( PyObject *self, PyObject *args );
static PyObject *Pymcsrch_mcsrch( PymcsrchObject *self, PyObject *args );
static void Pymcsrch_dealloc( PymcsrchObject *self );
static PyObject *Pymcsrch_getattr( PymcsrchObject *self, char *name );

extern void MCSRCH( int *n, double *x, double *f, double *g, double *s,
                    double *stp, double *ftol, double *gtol, double *xtol,
                    double *stpmin, double *stpmax, int *maxfev, int *info,
                    int *nfev, double *wa );

/* ========================================================================== */

/*
 *                    M o d u l e   f u n c t i o n s
 */

/* ========================================================================== */

static PymcsrchObject *NewPymcsrchObject( int n, double ftol, double gtol,
                                          double xtol, double stp,
                                          double stpmin, double stpmax,
                                          int maxfev, double *d ) {

    PymcsrchObject *self;

    /* Create new instance of object */
    if( !(self = PyObject_New( PymcsrchObject, &PymcsrchType ) ))
        return NULL; //PyErr_NoMemory( );

    /* Populate Pymcsrch data structure */
    self->n = n;
    if( !(self->wa = (double *)malloc( n * sizeof( double ) ))) return NULL;
    self->ftol = ftol;
    self->gtol = gtol;
    self->xtol = xtol;
    self->stp  = stp;
    self->stpmin = stpmin;
    self->stpmax = stpmax;
    self->maxfev = maxfev;
    self->dk = d;
    self->nfev = 0;
    self->info = 99;

    return self;
}

/* ========================================================================== */

static char Pymcsrch_mcsrch_Doc[] = "Perform modified More-Thuente linesearch";

static PyObject *Pymcsrch_mcsrch( PymcsrchObject *self, PyObject *args ) {

    PyArrayObject *a_x, *a_g;
    double f, *x, *g;

    /* Obtain f, x, g and d */
    if( !PyArg_ParseTuple( args,
                           "dO!O!",
                           &f,
                           &PyArray_Type, &a_x,
                           &PyArray_Type, &a_g )) return NULL;

    /* Check input */
    if( a_x->descr->type_num != NPY_DOUBLE ) return NULL;
    if( !a_x ) return NULL;                               /* conversion error */
    if( a_x->nd != 1 ) return NULL;                /* x must have 1 dimension */
    if( a_x->dimensions[0] != self->n ) return NULL;            /* and size n */
    PyArray_XDECREF( a_x );

    if( a_g->descr->type_num != NPY_DOUBLE ) return NULL;
    if( !a_g ) return NULL;                               /* conversion error */
    if( a_g->nd != 1 ) return NULL;                /* g must have 1 dimension */
    if( a_g->dimensions[0] != self->n ) return NULL;            /* and size n */
    PyArray_XDECREF( a_g );

    /* Assign input to variables x and g */
    x = (double *)a_x->data;
    g = (double *)a_g->data;

    /* Perform linesearch */
    MCSRCH( &(self->n),
            x,
            &f,
            g,
            self->dk,
            &(self->stp),
            &(self->ftol),
            &(self->gtol),
            &(self->xtol),
            &(self->stpmin),
            &(self->stpmax),
            &(self->maxfev),
            &(self->info),
            &(self->nfev),
            self->wa );

    return Py_BuildValue( "di", self->stp, self->info );
}

/* ========================================================================== */

/* This is necessary as Pymcsrch_mcsrch takes a PymcsrchObject* as argument */

static PyMethodDef Pymcsrch_special_methods[] = {
  { "mcsrch", (PyCFunction)Pymcsrch_mcsrch, METH_VARARGS, Pymcsrch_mcsrch_Doc },
  { NULL,     NULL,                         0,            NULL                }
};

/* ========================================================================== */

static char Pymcsrch_Init_Doc[] = "Initialize modified More-Thuente linesearch";

static PyObject *Pymcsrch_Init( PyObject *self, PyObject *args ) {

    /* Input must be n, ftol, gtol, xtol, stp, stpmin, stpmax */

    PymcsrchObject  *rv;                    /* Return value */
    PyArrayObject   *a_d;
    int              n, maxfev;
    double           ftol, gtol, xtol, stp, stpmin, stpmax;

    if( !PyArg_ParseTuple( args, "iddddddiO!",
                           &n, &ftol, &gtol, &xtol,
                           &stp, &stpmin, &stpmax, &maxfev,
                           &PyArray_Type, &a_d ) )
        return NULL;

    /* Check input array */
    if( a_d->descr->type_num != NPY_DOUBLE ) return NULL;
    if( !a_d ) return NULL;                               /* conversion error */
    if( a_d->nd != 1 ) return NULL;                /* d must have 1 dimension */
    if( a_d->dimensions[0] != n ) return NULL;            /* and size n */
    PyArray_XDECREF( a_d );

    /* Spawn new Pymcsrch Object */
    rv = NewPymcsrchObject( n, ftol, gtol, xtol,
                            stp, stpmin, stpmax, maxfev,
                            (double *)a_d->data );
    if( rv == NULL ) return NULL;

    return (PyObject *)rv;
}

/* ========================================================================== */

/*
 *    D e f i n i t i o n   o f   P y c s r c h   c o n t e x t   t y p e
 */

/* ========================================================================== */

static PyTypeObject PymcsrchType = {
    //PyObject_HEAD_INIT(NULL)
    //0,
    PyVarObject_HEAD_INIT(NULL, 0)
    "pymcsrch_context",
    sizeof(PymcsrchObject),
    0,
    (destructor)Pymcsrch_dealloc,  /* tp_dealloc */
    0,                            /* tp_print */
    0,//(getattrfunc)Pymcsrch_getattr, /* tp_getattr */
    0,                            /* tp_setattr */
    0,                            /* tp_compare */
    0,                            /* tp_repr */
    0,                            /* tp_as_number*/
    0,                            /* tp_as_sequence*/
    0,                            /* tp_as_mapping*/
    0,                            /* tp_hash */
    0,                            /* tp_call*/
    0,                            /* tp_str*/
    (getattrofunc)Pymcsrch_getattr,                            /* tp_getattro*/
    0,                            /* tp_setattro*/
    0,                            /* tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,           /* tp_flags*/
    "Pymcsrch Context Object",     /* tp_doc */
};

/* ========================================================================== */

/*
 *        D e f i n i t i o n   o f   P y c s r c h   m e t h o d s
 */

/* ========================================================================== */

static PyMethodDef PymcsrchMethods[] = {
    { "Init",   Pymcsrch_Init,    METH_VARARGS, Pymcsrch_Init_Doc   },
    //{ "mcsrch", (PyCFunction)Pymcsrch_mcsrch, METH_VARARGS, Pymcsrch_mcsrch_Doc },
    { NULL,     NULL,             0,            NULL                }
};

/* ========================================================================== */

static void Pymcsrch_dealloc( PymcsrchObject *self ) {

    free( self->wa );
    PyObject_Del(self);
}

/* ========================================================================== */

static PyObject *Pymcsrch_getattr( PymcsrchObject *self, char *name ) {

    if( strcmp( name, "shape" ) == 0 )
        return Py_BuildValue( "(i,i)", 0, 0 );
    if( strcmp( name, "nnz" ) == 0 )
        return Py_BuildValue( "i", 0 );
    if( strcmp( name, "__members__" ) == 0 ) {
        char *members[] = {"shape", "nnz"};
        int i;

        PyObject *list = PyList_New( sizeof(members)/sizeof(char *) );
        if( list != NULL ) {
            for( i = 0; i < sizeof(members)/sizeof(char *); i++ )
                PyList_SetItem( list, i, PyUnicode_FromString(members[i]) );
            if( PyErr_Occurred() ) {
                Py_DECREF( list );
                list = NULL;
            }
        }
        return list;
    }
    //fprintf(stderr, "Trying to return method.\n");
    //return PyObject_GenericGetAttr( (PyObject *) self, PyUnicode_FromString(name) );
    //return Py_FindMethod( Pymcsrch_special_methods, (PyObject *)self, name );
    PyObject *search_function = PyCFunction_New(&Pymcsrch_special_methods[0], (PyObject *)self);
    //fprintf(stderr, "Created new function.\n");
    if (search_function==NULL) {
        fprintf(stderr, "Tried to find mcsrch function, but function pointer null.\n");
        search_function=Py_None;
    }
    Py_INCREF(search_function);
    //fprintf(stderr, "Returning after getting function.");
    return search_function;
}

/* ========================================================================== */

//int init_pymcsrch( void ) {
PyObject* PyInit__pymcsrch( void ) {

    PyObject *m, *d;

    if( PyType_Ready( &PymcsrchType ) < 0 ) return NULL;
    /*
    m = Py_InitModule3( "_pymcsrch",
                        PymcsrchMethods,
                        "Python interface to mcsrch" );
    */
    static struct PyModuleDef moduledef = {
        PyModuleDef_HEAD_INIT,
        "_pymcsrch",                         /* m_name */
        "Python interface to mcsrch",       /* m_doc */
        -1,                                 /* m_size */
        PymcsrchMethods,                    /* m_methods */
        NULL,                               /* m_reload */
        NULL,                               /* m_traverse */
        NULL,                               /* m_clear */
        NULL,                               /* m_free */
    };

    m = PyModule_Create(&moduledef);
    if (m == NULL)
        return NULL;
    
    import_array( );         /* Initialize the Numarray module. */

    d = PyModule_GetDict(m);
    PyDict_SetItemString(d, "PymcsrchType", (PyObject *)&PymcsrchType);


    /* Check for errors */
    if (PyErr_Occurred())
        Py_FatalError("Unable to initialize module pymcsrch");

    return m;
}

/* ========================================================================== */

