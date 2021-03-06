#ifndef JSLINT_H
#define JSLINT_H

#include <QObject>
#include <QString>
#include <QJsonDocument>
#include <QJSEngine>

class JsLint : public QObject
{
public:
    JsLint(QObject *parent = 0);
    virtual ~JsLint();

    QJsonDocument lint(const QString &code);

private:
    bool setup(const QString &lintPath);

    QJSEngine m_engine;
    QJSValue m_lintFunction;
};

#endif // JSLINT_H
