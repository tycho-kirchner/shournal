
#include <cstdlib>
#include <QTextStream>
#include <QStandardPaths>

#include "console_dialog.h"

#include "qoutstream.h"
#include "util.h"
#include "subprocess.h"

using subprocess::Subprocess;

/// Ask a simple yesno-question and return the result.
/// @returns true, if "y", false if "n" was entered
bool console_dialog::yesNo(const QString &question)
{
    const QString yesStr = qtr("y");
    const QString noStr = qtr("n");
    QOut() << QString("%1 (%2/%3) ").arg(question, yesStr, noStr);

    QTextStream input(stdin);
    while (true) {
        QString respone = input.readLine();
        if(respone.compare(yesStr, Qt::CaseSensitivity::CaseInsensitive) == 0){
            return true;
        }
        if(respone.compare(noStr, Qt::CaseSensitivity::CaseInsensitive) == 0){
            return false;
        }
        QOut() << qtr("Please enter %1 or %2").arg(yesStr, noStr) << "\n";
    }

}

/// Open filepath within the users favourite editor,
/// exported in environment variable EDITOR. If not set, try
/// to find a typical editor such as nano, vim,...
/// @return return value of the launched process. In case it did'nt exit normally,
///         an os-exception is thrown,
/// @throws QExcIo, os::ExcOs
int console_dialog::openFileInExternalEditor(const QString &filepath)
{
    QString editor = getenv("EDITOR");
    if(editor.isEmpty()){
        if((editor=QStandardPaths::findExecutable("nano")).isEmpty())
            if((editor=QStandardPaths::findExecutable("vim")).isEmpty())
                if((editor=QStandardPaths::findExecutable("vi")).isEmpty()){
                    throw QExcIo(qtr("No texteditor found, please set EDITOR "
                                     "environment variable."));
                }
    }
    Subprocess subproc;
    subproc.call({editor.toStdString(), filepath.toStdString()});
    return subproc.waitFinish();
}
