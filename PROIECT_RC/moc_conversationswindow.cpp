/****************************************************************************
** Meta object code from reading C++ file 'conversationswindow.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.13)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "conversationswindow.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'conversationswindow.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.13. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_ConversationsWindow_t {
    QByteArrayData data[13];
    char stringdata0[167];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_ConversationsWindow_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_ConversationsWindow_t qt_meta_stringdata_ConversationsWindow = {
    {
QT_MOC_LITERAL(0, 0, 19), // "ConversationsWindow"
QT_MOC_LITERAL(1, 20, 12), // "returnToMain"
QT_MOC_LITERAL(2, 33, 0), // ""
QT_MOC_LITERAL(3, 34, 9), // "onRefresh"
QT_MOC_LITERAL(4, 44, 13), // "onCreateGroup"
QT_MOC_LITERAL(5, 58, 17), // "loadConversations"
QT_MOC_LITERAL(6, 76, 10), // "loadGroups"
QT_MOC_LITERAL(7, 87, 17), // "onOpenPrivateChat"
QT_MOC_LITERAL(8, 105, 16), // "QListWidgetItem*"
QT_MOC_LITERAL(9, 122, 4), // "item"
QT_MOC_LITERAL(10, 127, 15), // "onOpenGroupChat"
QT_MOC_LITERAL(11, 143, 16), // "onReturnFromChat"
QT_MOC_LITERAL(12, 160, 6) // "onExit"

    },
    "ConversationsWindow\0returnToMain\0\0"
    "onRefresh\0onCreateGroup\0loadConversations\0"
    "loadGroups\0onOpenPrivateChat\0"
    "QListWidgetItem*\0item\0onOpenGroupChat\0"
    "onReturnFromChat\0onExit"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_ConversationsWindow[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       9,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    0,   59,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       3,    0,   60,    2, 0x08 /* Private */,
       4,    0,   61,    2, 0x08 /* Private */,
       5,    0,   62,    2, 0x08 /* Private */,
       6,    0,   63,    2, 0x08 /* Private */,
       7,    1,   64,    2, 0x08 /* Private */,
      10,    1,   67,    2, 0x08 /* Private */,
      11,    0,   70,    2, 0x08 /* Private */,
      12,    0,   71,    2, 0x08 /* Private */,

 // signals: parameters
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 8,    9,
    QMetaType::Void, 0x80000000 | 8,    9,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

void ConversationsWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<ConversationsWindow *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->returnToMain(); break;
        case 1: _t->onRefresh(); break;
        case 2: _t->onCreateGroup(); break;
        case 3: _t->loadConversations(); break;
        case 4: _t->loadGroups(); break;
        case 5: _t->onOpenPrivateChat((*reinterpret_cast< QListWidgetItem*(*)>(_a[1]))); break;
        case 6: _t->onOpenGroupChat((*reinterpret_cast< QListWidgetItem*(*)>(_a[1]))); break;
        case 7: _t->onReturnFromChat(); break;
        case 8: _t->onExit(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (ConversationsWindow::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&ConversationsWindow::returnToMain)) {
                *result = 0;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject ConversationsWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_ConversationsWindow.data,
    qt_meta_data_ConversationsWindow,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *ConversationsWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ConversationsWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ConversationsWindow.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int ConversationsWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 9)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 9;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 9)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 9;
    }
    return _id;
}

// SIGNAL 0
void ConversationsWindow::returnToMain()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
