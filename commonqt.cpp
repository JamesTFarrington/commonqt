// -*- c-basic-offset: 8; indent-tabs: nil -*-
//
// See LICENSE for details.
//
#include <smoke.h>
#include <smoke/qtcore_smoke.h>
#include <QtCore>
#include <QtGui>
#include "commonqt.h"

// #define DEBUG 1

#include <iostream>
#include <string>
#include <stdlib.h>

using namespace std;

typedef void (*t_deletion_callback)(void*, void*);
typedef bool (*t_callmethod_callback)(void*, short, void*, void*);
typedef bool (*t_dynamic_callmethod_callback)(void*, short, short, void*, void*);
typedef void (*t_child_callback)(void*, bool, void*);

Smoke* smoke_modules[16];
unsigned char n_smoke_modules = 0;

class Binding : public SmokeBinding
{
public:
        Binding(Smoke* s) : SmokeBinding(s) {}

        t_deletion_callback deletion_callback;
        t_callmethod_callback callmethod_callback;
	t_child_callback child_callback;

        void deleted(Smoke::Index, void* obj) {
                deletion_callback(smoke, obj);
        }

        bool callMethod(Smoke::Index method, void* obj,
                Smoke::Stack args, bool)
        {
		Smoke::Method* m = &smoke->methods[method];
		const char* name = smoke->methodNames[m->name];
		// Smoke::Class* c = &smoke->classes[m->classId];

		if (*name == '~')
			callmethod_callback(smoke, method, obj, args);
		// else if (!strcmp(name, "notify")
		// 	 && (!strcmp(c->className, "QApplication")
		// 	    || !strcmp(c->className, "QCoreApplication")))
		// {
		// 	QEvent* e = (QEvent*) args[2].s_voidp;
		// 	if (e->type() == QEvent::ChildAdded
		// 	    || e->type() == QEvent::ChildRemoved)
		// 	{
		// 		QChildEvent* f = (QChildEvent*) e;
		// 		child_callback(smoke, f->added(), f->child());
		// 	}
		// }
		return false;
	}

        char* className(Smoke::Index classId) {
                return (char*) smoke->classes[classId].className;
        }
};

class DynamicBinding : public Binding
{
public:
        DynamicBinding(Smoke* s) : Binding(s) {}

        QHash<short, short> overridenMethods;
        QMetaObject* metaObject;
        short metaObjectIndex;
        t_dynamic_callmethod_callback callmethod_callback;

        bool callMethod(Smoke::Index method, void* obj,
                Smoke::Stack args, bool)
        {
                
                if (method == metaObjectIndex) {
                        args[0].s_voidp = (void*)metaObject;
                        return true;
                }
                else if (overridenMethods.contains(method)) {
                        short override_index = overridenMethods.value(method);
                        
                        return callmethod_callback(smoke, method,
                                                   override_index,
                                                   obj, args);
                }
                else {
                        return false;
                }
        }
};

void
sw_smoke(Smoke* smoke,
	 SmokeData* data,
	 void* deletion_callback,
	 void* method_callback,
	 void* child_callback)
{
        Binding* binding = new Binding(smoke);
        
        smoke_modules[n_smoke_modules++] = smoke;

	data->name = smoke->moduleName();

        data->classes = smoke->classes;
        data->numClasses = smoke->numClasses;

        data->methods = smoke->methods;
        data->numMethods = smoke->numMethods;

        data->methodMaps = smoke->methodMaps;
        data->numMethodMaps = smoke->numMethodMaps;

        data->methodNames = smoke->methodNames;
        data->numMethodNames = smoke->numMethodNames;

        data->types = smoke->types;
        data->numTypes = smoke->numTypes;

        data->inheritanceList = smoke->inheritanceList;
        data->argumentList = smoke->argumentList;
        data->ambiguousMethodList = smoke->ambiguousMethodList;
        data->castFn = (void *) smoke->castFn;

	binding->deletion_callback
		= (t_deletion_callback) deletion_callback;

        binding->callmethod_callback
                = (t_callmethod_callback) method_callback;

	binding->child_callback
		= (t_child_callback) child_callback;

        data->binding = binding;
}

void sw_override(DynamicBinding* dynamicBinding, short method,
                 short override_index)
{
        dynamicBinding->overridenMethods[method] = override_index;
}

void* sw_make_dynamic_binding(Smoke* smoke,
                              QMetaObject* metaObject,
                              short metaObjectIndex,
                              void* deletion_callback,
                              void* method_callback,
                              void* child_callback) {
        DynamicBinding* dynamicBinding = new DynamicBinding(smoke);

        dynamicBinding->deletion_callback
		= (t_deletion_callback) deletion_callback;

        dynamicBinding->callmethod_callback
                = (t_dynamic_callmethod_callback) method_callback;

	dynamicBinding->child_callback
		= (t_child_callback) child_callback;

        dynamicBinding->metaObject = metaObject;
        dynamicBinding->metaObjectIndex = metaObjectIndex;
        return dynamicBinding;
}

int
sw_windows_version()
{
#ifdef WINDOWS
	return QSysInfo::windowsVersion();
#else
	return -1;
#endif
}

// void*
// sw_make_qstring(char *str)
// {
//         QString* qstr = new QString();
//         *qstr = QString::fromUtf8(str);
//         return qstr;
// }

void*
sw_make_qbytearray(char* str)
{
        return new QByteArray(str);
}

void
sw_delete_qbytearray(void *q)
{
        delete (QByteArray*) q;
}

void*
sw_make_qstring(char *str)
{
        return new QString(QString::fromUtf8(str));
}

void*
sw_qstring_to_utf8(void* s)
{
        QString* str = (QString*) s;
        return new QByteArray(str->toUtf8());
}

const void*
sw_qstring_to_utf16(void* s)
{
        QString* str = static_cast<QString*>(s);
	return str->utf16();
}

void
sw_delete_qstring(void *q)
{
        delete (QString*) q;
}

void*
sw_make_metaobject(void *p, char *strings, int *d)
{
        QMetaObject* parent = (QMetaObject*) p;
        const uint* data = (const uint*) d;

        QMetaObject tmp = { { parent, strings, data, 0 } };
        QMetaObject* ptr = new QMetaObject;
        *ptr = tmp;
        return ptr;
}

void
sw_delete(void *p)
{
        QObject* q = (QObject*) p;
        delete q;
}

typedef void (*t_ptr_callback)(void *);

inline short sw_module_index(Smoke* module)
{
        for (short i = 0; i < n_smoke_modules; i++) {
                if (smoke_modules[i] == module)
                        return i;
        }
        return -1;
}

enum qkind {QCLASS, QMEHTOD, QMETHODMAP, QTYPE};

#define KIND_BITS 2
#define MODULE_BITS 4
#define INDEX_BITS 16

inline
unsigned bash(short index, short moduleIndex, qkind kind)
{
        return kind | ((moduleIndex | (index << MODULE_BITS)) << KIND_BITS);
}

inline
short unbash(unsigned x, short& module)
{
        module = x >> 2 & ((1 << MODULE_BITS) - 1);
        return x >> (MODULE_BITS + KIND_BITS);
}

void
sw_find_class(char *name, Smoke **smoke, short *index)
{
	Smoke::ModuleIndex mi = qtcore_Smoke->findClass(name);
	*smoke = mi.smoke;
	*index = mi.index;
}

unsigned sw_resolve_external_qclass(unsigned x)
{
        short module;
        short index = unbash(x, module);

        Smoke* smoke = smoke_modules[module];
        Smoke::Class qclass = smoke->classes[index];
        if (qclass.external)
                return sw_find_class_2(qclass.className);
        else
                return x;
        
}

unsigned
sw_find_class_2(const char *name)
{
	Smoke::ModuleIndex mi = qtcore_Smoke->findClass(name);
        if (mi.smoke == NULL)
                return 0;
        return bash(mi.index, sw_module_index(mi.smoke), QCLASS);
}

void
sw_id_instance_class(void *ptr, Smoke **smoke, short *index)
{
	Smoke::ModuleIndex mi = qtcore_Smoke->findClass(((QObject*)ptr)->metaObject()->className());
	*smoke = mi.smoke;
	*index = mi.index;
}

short
sw_find_name(Smoke *smoke, char *name)
{
	Smoke::ModuleIndex mi = smoke->idMethodName(name);
	return mi.index;
}

unsigned
sw_find_name_index_range(Smoke *smoke, char* name)
{
        Smoke::ModuleIndex mi = smoke->idMethodName(name);
        short number = smoke->numMethodNames;
        short max;
        if (mi.index == 0)
                return 0;
        for (max = mi.index + 1; max < number; max++) {
                for (int c = 0;; c++) {
                        char c1 = name[c];
                        char c2 = smoke->methodNames[max][c];
                        if (c1 != c2) {
                                if (c2 == '$' || c2 == '#' || c2 == '?')
                                        break;
                                else
                                        return mi.index | (max - 1) << 16;
                        }
                                
                                
                        if (c1 == 0 || c2 == 0)
                                break;
                }
        }
	return mi.index | (max - 1) << 16;
}

inline int leg(short a, short b) {  // ala Perl's <=>
	if(a == b) return 0;
	return (a > b) ? 1 : -1;
}

short
sw_find_any_methodmap(Smoke *smoke, short classIndex, short min, short max)
{
        short imax = smoke->numMethodMaps;
        short imin = 1;
        short icur = -1;
        int icmp = -1;

        while (imax >= imin) {
                icur = (imin + imax) / 2;
                icmp = leg(smoke->methodMaps[icur].classId, classIndex);
                if (icmp == 0) {
                        short name = smoke->methodMaps[icur].name;

                        if (min > name)
                                imin = icur + 1;
                        else if (max < name)
                                imax = icur - 1;
                        else
                                return icur;
                        
                } else if (icmp > 0) {
                        imax = icur - 1;
                } else {
                        imin = icur + 1;
                }
        }

        return 0;
}

short
sw_id_method(Smoke *smoke, short classIndex, short methodIndex)
{
	Smoke::ModuleIndex mi = smoke->idMethod(classIndex, methodIndex);
	return mi.index;
}

short
sw_id_type(Smoke *smoke, char *name)
{
	return smoke->idType(name);
}

short
sw_id_class(Smoke *smoke, char *name, bool external)
{
	return smoke->idClass(name, external).index;
}

// QStringList marshalling

void*
sw_qstringlist_new()
{
        return new QStringList();
}

int
sw_qstringlist_size(void* q)
{
	return static_cast<QStringList*>(q)->size();
}

void
sw_qstringlist_delete(void *q)
{
	delete static_cast<QStringList*>(q);
}

const void*
sw_qstringlist_at(void* q, int index)
{
	return static_cast<QStringList*>(q)->at(index).utf16();
}

void
sw_qstringlist_append(void *q, char *x)
{
	static_cast<QStringList*>(q)->append(QString(QString::fromUtf8(x)));
}

// QList<Something*> marshalling

void*
sw_qlist_void_new()
{
	return new QList<void*>;
}

int
sw_qlist_void_size(void *ptr)
{
	QList<void*>* qlist = static_cast<QList<void*>*>(ptr);
	return qlist->size();
}

void
sw_qlist_void_delete(void *ptr)
{
	QList<void*>* qlist = static_cast<QList<void*>*>(ptr);
	delete qlist;
}

const void*
sw_qlist_void_at(void *ptr, int index)
{
	QList<void*>* qlist = static_cast<QList<void*>*>(ptr);
	return qlist->at(index);
}

void
sw_qlist_void_append(void *ptr, void *whatptr)
{
	QList<void*>* qlist = static_cast<QList<void*>*>(ptr);
	qlist->append(whatptr);
}

// QList<scalar> marshalling

#define DEFINE_QLIST_SCALAR_MARSHALLER(ELEMENT_TYPE, NAME) \
  void* \
  sw_qlist_##NAME##_new() \
  { \
          return new QList<ELEMENT_TYPE>; \
  } \
  int \
  sw_qlist_##NAME##_size(void *ptr) \
  { \
        QList<ELEMENT_TYPE>* qlist = static_cast<QList<ELEMENT_TYPE>*>(ptr); \
  	return qlist->size(); \
  } \
  void \
  sw_qlist_##NAME##_delete(void *ptr) \
  { \
  	QList<ELEMENT_TYPE>* qlist = static_cast<QList<ELEMENT_TYPE>*>(ptr); \
  	delete qlist; \
  } \
  const void* \
  sw_qlist_##NAME##_at(void *ptr, int index) \
  { \
  	QList<ELEMENT_TYPE>* qlist = static_cast<QList<ELEMENT_TYPE>*>(ptr); \
  	return &qlist->at(index); \
  } \
  void \
  sw_qlist_##NAME##_append(void *ptr, void *whatptr) \
  { \
  	QList<ELEMENT_TYPE>* qlist = static_cast<QList<ELEMENT_TYPE>*>(ptr); \
  	qlist->append(*static_cast<ELEMENT_TYPE*>(whatptr)); \
  }

DEFINE_QLIST_SCALAR_MARSHALLER(int, int)
DEFINE_QLIST_SCALAR_MARSHALLER(QVariant, qvariant)
DEFINE_QLIST_SCALAR_MARSHALLER(QByteArray, qbytearray)
DEFINE_QLIST_SCALAR_MARSHALLER(QModelIndex, qmodelindex)
DEFINE_QLIST_SCALAR_MARSHALLER(QKeySequence, qkeysequence)
