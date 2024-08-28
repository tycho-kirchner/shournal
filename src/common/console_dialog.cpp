
#include <cstdlib>
#include <QTextStream>
#include <QStandardPaths>


#include "compat.h"
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
    subprocess::Args_t args;
    if(editor.isEmpty()){
        if((editor=QStandardPaths::findExecutable("nano")).isEmpty())
            if((editor=QStandardPaths::findExecutable("vim")).isEmpty())
                if((editor=QStandardPaths::findExecutable("vi")).isEmpty()){
                    throw QExcIo(qtr("No texteditor found, please set EDITOR "
                                     "environment variable."));
                }
        args.push_back(editor.toStdString());
    } else {
        // support also EDITOR-strings like e.g. 'geany -i' -> if we cannot find
        // the executable, try to split by space
        if((QStandardPaths::findExecutable(editor)).isEmpty() ){
            const auto splitted = editor.split(' ', Qt::SkipEmptyParts);
            if(splitted.length() > 1){
                for(const QString& s : splitted){
                    args.push_back(s.toStdString());
                }
            } else {
                // let it (probably) fail below:
                args.push_back(editor.toStdString());
            }
        } else {
            args.push_back(editor.toStdString());
        }
    }
    args.push_back(filepath.toStdString());
    Subprocess subproc;
    subproc.call(args);
    return subproc.waitFinish();
}
