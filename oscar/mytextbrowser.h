#ifndef MYTEXTBROWSER_H
#define MYTEXTBROWSER_H

#include <QTextBrowser>

class MyTextBrowser:public QTextBrowser
{
    Q_OBJECT
public:
    MyTextBrowser(QWidget * parent):QTextBrowser(parent) {}
    virtual ~MyTextBrowser() {}
    virtual QVariant loadResource(int type, const QUrl &url) Q_DECL_OVERRIDE;
};


#endif // MYTEXTBROWSER_H
