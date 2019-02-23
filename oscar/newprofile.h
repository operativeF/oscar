/* Create New Profile Header
 *
 * Copyright (C) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef NEWPROFILE_H
#define NEWPROFILE_H

#include <QDialog>
#include <QUrl>

namespace Ui {
class NewProfile;
}

/*! \class NewProfile
    \author Mark Watkins <jedimark_at_users.sourceforge.net>
    \brief Profile creation/editing wizard
    */
class NewProfile : public QDialog
{
    Q_OBJECT

  public:
    explicit NewProfile(QWidget *parent = 0, const QString *user = 0);
    ~NewProfile();

    //! \brief When used in edit mode, this skips the first page
    void skipWelcomeScreen();

    //! \brief Open profile named 'name' for editing, loading all it's content
    void edit(const QString name);

  private slots:
    //! \brief Validate each step and move to the next page, saving at the end if requested.
    void on_nextButton_clicked();

    //! \brief Go back to the previous wizard page
    void on_backButton_clicked();

    void on_cpapModeCombo_activated(int index);

    void on_agreeCheckbox_clicked(bool checked);

    void on_passwordEdit1_editingFinished();

    void on_passwordEdit2_editingFinished();

    void on_heightCombo_currentIndexChanged(int index);

    void on_textBrowser_anchorClicked(const QUrl &arg1);

  private:
    QString getIntroHTML();

    Ui::NewProfile *ui;
    bool m_editMode;
    int m_firstPage;
    bool m_passwordHashed;
};

#endif // NEWPROFILE_H
