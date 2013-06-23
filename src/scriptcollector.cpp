#include "scriptcollector.h"

#include <QDebug>

#include <private/qqmldata_p.h>
#include <private/qqmltypeloader_p.h>
#include <private/qqmlscript_p.h>

inline bool isLineTerminatorSequence(const QChar &c)
{
    switch (c.unicode()) {
    case 0x000Au:
    case 0x2028u:
    case 0x2029u:
    case 0x000Du:
        return true;
    default:
        return false;
    }
}

void mapOffsetToLineAndColumn(const QString &data, const quint32 &offset, quint16 &line, quint16 &column)
{
    line = 1;
    column = 1;

    if (data.length() <= (int)offset) {
        return;
    }

    for (quint32 index = 0; index <= offset; ++index) {
        ++column;
        if (isLineTerminatorSequence(data.at(index))) {
            ++line;
            column = 1;
        }
    }
}

ScriptCollector::ScriptCollector() :
    m_objectStartOffset(0)
{
}

ScriptCollector::~ScriptCollector()
{
}

bool ScriptCollector::parse(const QString &data,
                            const QUrl &url = QUrl(),
                            const QString &urlString = QString())
{
    QQmlScript::Parser parser;
    parser.parse(data, QByteArray(), url, urlString);

    m_errors += parser.errors();

    if (parser.errors().isEmpty()) {
        qDebug() << "Parsed" << urlString << "successfully";
        collectJS(parser.tree(), data);
        determineObjectStartOffset(data, parser.root);
        qSort(m_scripts);
        return true;
    } else {
        m_errors = parser.errors();
        return false;
    }
}

void ScriptCollector::clear()
{
    m_errors.clear();
    m_scripts.clear();
}

QList<QQmlError> ScriptCollector::errors() const
{
    return m_errors;
}

QList<ScriptCollector::Script> ScriptCollector::scripts() const
{
    return m_scripts;
}

quint32 ScriptCollector::objectStartOffset() const
{
    return m_objectStartOffset;
}

inline QString toString(const QHashedStringRef& ref)
{
    return QString(ref.constData(), ref.length());
}

// This is really ugly, should use the real parser instead... but no internal access
void ScriptCollector::determineObjectStartOffset(const QString &data, QQmlScript::Object *node)
{    
    static QLatin1Char CHAR_SLASH('/');
    static QLatin1Char CHAR_STAR('*');
    static QLatin1Char CHAR_CURLY_OPEN('{');
    m_objectStartOffset = node->location.range.offset;

    bool multilineComment = false;
    bool singleLineComment = false;
    for (quint32 index = node->location.range.offset,
                 length = node->location.range.offset + node->location.range.length;
                 index <= length; ++index) {
        ++m_objectStartOffset;

        if (!multilineComment && !singleLineComment) {
            if (data.at(index) == CHAR_SLASH && data.at(index + 1) == CHAR_SLASH) {
                singleLineComment = true;
            } else if (data.at(index) == CHAR_SLASH && data.at(index + 1) == CHAR_STAR) {
                multilineComment = true;
            } else if (data.at(index) == CHAR_CURLY_OPEN) {
                return;
            }
        }

        if (data.at(index) == CHAR_STAR && data.at(index + 1) == CHAR_SLASH) {
            multilineComment = false;
        }

        if (isLineTerminatorSequence(data.at(index))) {
            singleLineComment = false;
        }
    }
}

void ScriptCollector::collectJS(QQmlScript::Object *node, const QString &data)
{
    // main properties
    QQmlScript::Property *prop = node->properties.first();
    while (prop != 0) {

        QQmlScript::Value *value = prop->values.first();
        while (value != 0) {

            if (toString(prop->name()) != "id") {
                if (value->object) {
                    // properties with qml objects
                    collectJS(value->object, data); // recurse
                } else if (value->value.isScript()) {

                    // qDebug().nospace() << ">>>>> " << toString(prop->name()) << " ("
                    //                   << value->location.start.line << ":" << value->location.start.column << " to "
                    //                   << value->location.end.line << ":" << value->location.end.column << ")";

                    // Javascript ->
                    // qDebug() << value->value.asScript();

                    Script script = {toString(prop->name()),
                                     value->value.asScript(),
                                     ScriptCollector::Property,
                                     { { value->location.start.line, value->location.start.column },
                                       { value->location.end.line, value->location.end.column },
                                       { value->location.range.offset, value->location.range.length } } };
                    m_scripts.append(script);
                }
            }

            value = prop->values.next(value);
        }

        prop = node->properties.next(prop);
    }

    // child-objects
    if (node->defaultProperty) {

        QQmlScript::Value *value = node->defaultProperty->values.first();
        while (value != 0) {

            if (value->object) {
                collectJS(value->object, data); // recurse
            }
            value = node->defaultProperty->values.next(value);
        }
    }

    // all other properties
    QQmlScript::Object::DynamicProperty *dprop = node->dynamicProperties.first();
    while (dprop != 0) {

        bool isAlias = (dprop->type == QQmlScript::Object::DynamicProperty::Alias);

        QQmlScript::Property *prop = dprop->defaultValue;
        if (prop && !isAlias) {

            QQmlScript::Value *value = prop->values.first();
            while (value != 0) {

                if (value->value.isScript()) {
                    // qDebug().nospace() << ">>>>> " << toString(dprop->name) << " ("
                    //                   << value->location.start.line << ":" << value->location.start.column << " to "
                    //                   << value->location.end.line << ":" << value->location.end.column << ")";

                    // Javascript ->
                    // qDebug() << value->value.asScript();

                    Script script = {toString(dprop->name),
                                     value->value.asScript(),
                                     ScriptCollector::Property,
                                     { { value->location.start.line, value->location.start.column },
                                       { value->location.end.line, value->location.end.column },
                                       { value->location.range.offset, value->location.range.length } } };
                    m_scripts.append(script);
                }

                value = prop->values.next(value);
            }
        }

        dprop = node->dynamicProperties.next(dprop);
    }

    // functions
    QQmlScript::Object::DynamicSlot *dslot = node->dynamicSlots.first();
    while (dslot != 0) {
        // qDebug().nospace() << ">>>>> " << toString(dslot->name) << " ("
        //                   << dslot->location.start.line << ":" << dslot->location.start.column << " to "
        //                   << dslot->location.end.line << ":" << dslot->location.end.column << ")";

        // Javascript ->
        // inner code is dslot->body, without function tag & name & params
        // qDebug() << dslot->body;

        // the location contains the functionName, but body does not, this needs to be adapted
        quint32 offset = dslot->location.range.length - dslot->body.length();
        quint16 line, column;
        mapOffsetToLineAndColumn(data, dslot->location.range.offset + offset, line, column);

        Script script = {toString(dslot->name),
                         dslot->body,
                         ScriptCollector::Function,
                         { { line, column },
                           { dslot->location.end.line, dslot->location.end.column },
                           { dslot->location.range.offset + offset, dslot->location.range.length - offset } } };
        m_scripts.append(script);

        // qDebug() << script << line << column;

        dslot = node->dynamicSlots.next(dslot);
    }
}

QDebug operator<<(QDebug dbg, const ScriptCollector::Script &script)
{
    dbg.nospace() << script.name << " ("
                  << script.location.start.line << ":" << script.location.start.column << "-"
                  << script.location.end.line << ":" << script.location.end.column << ")";

    return dbg.space();
}
