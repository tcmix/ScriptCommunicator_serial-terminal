#include "parseThread.h"
#include <QFile>
#include <QFileInfo>
#include "mainwindow.h"
#include <QDirIterator>
#include <QTextStream>
#include <QCoreApplication>

/**
 * Returns the folder ich which the ScriptEditor files
 * are locared (api files).
 * @return
 *      The folder.
 */
QString getScriptEditorFilesFolder(void)
{
#ifdef Q_OS_MAC
    return QCoreApplication::applicationDirPath() + "/../../..";
#else
    return QCoreApplication::applicationDirPath();
#endif
}

/**
 * Parses a single line from an api file and adds functions which return objects to m_functionsWithResultObjects.
 * @param singleLine
 *      The current line
 */
void ParseThread::parseSingleLineForFunctionsWithResultObjects(QString singleLine)
{
    QStringList split = singleLine.split(":");
    if(split.length() >= 4)
    {
        FunctionWithResultObject element;
        element.isArray = false;
        QString className = split[0];
        element.functionName = split[2];
        element.resultType = split[3];
        int index = element.functionName.indexOf("(");
        if(index != -1)
        {
            element.functionName.remove(index + 1, element.functionName.length() - (index + 1));
        }
        else
        {//Invalid entry.
            return;
        }

        index = element.resultType.indexOf("\\n");
        if(index != -1)
        {
            element.resultType.remove(index, element.resultType.length() - index);
            element.resultType.replace(" ", "");//Remove all spaces

            if(element.resultType.startsWith("Array"))
            {
                element.isArray = true;

                if(element.resultType.startsWith("Array<String>"))
                {
                    element.resultType = "String";
                }
                else if(element.resultType.startsWith("Array<ScriptXmlAttribute>"))
                {
                    element.resultType = "ScriptXmlAttribute";
                }
                else if(element.resultType.startsWith("Array<ScriptXmlElement>"))
                {
                    element.resultType = "ScriptXmlElement";
                }
                else
                {
                    element.resultType = "Dummy";
                }
            }
            else
            {
                if(element.resultType.startsWith("String") ||
                   element.resultType.startsWith("ScriptSqlField") ||
                   element.resultType.startsWith("ScriptSqlRecord") ||
                   element.resultType.startsWith("ScriptSqlError") ||
                   element.resultType.startsWith("ScriptSqlRecord") ||
                   element.resultType.startsWith("ScriptSqlIndex") ||
                   element.resultType.startsWith("ScriptSqlQuery") ||
                   element.resultType.startsWith("ScriptSqlDatabase") ||
                   element.resultType.startsWith("ScriptXmlElement") ||
                   element.resultType.startsWith("ScriptXmlReader") ||
                   element.resultType.startsWith("ScriptXmlWriter") ||
                   element.resultType.startsWith("ScriptTimer") ||
                   element.resultType.startsWith("ScriptUdpSocket") ||
                   element.resultType.startsWith("ScriptTcpServer") ||
                   element.resultType.startsWith("ScriptTcpServer") ||
                   element.resultType.startsWith("ScriptSerialPort") ||
                   element.resultType.startsWith("ScriptCheetahSpi") ||
                   element.resultType.startsWith("ScriptPcanInterface") ||
                   element.resultType.startsWith("ScriptTcpClient") ||
                   element.resultType.startsWith("ScriptTreeWidgetItem") ||
                   element.resultType.startsWith("ScriptPlotWidget") ||
                   element.resultType.startsWith("ScriptPlotWindow") ||
                   element.resultType.startsWith("ScriptCanvas2DWidget") ||
                   element.resultType.startsWith("ScriptTcpClient"))
                {
                    //Return value has a valid class.
                }
                else
                {
                    return;
                }

            }

        }
        else
        {//Invalid entry.
            return;
        }


        if(!m_functionsWithResultObjects.contains((className)))
        {
            m_functionsWithResultObjects[className] = QVector<FunctionWithResultObject>();
        }


        m_functionsWithResultObjects[className].append(element);
    }
}

ParseThread::ParseThread(QObject *parent) : QThread(parent), m_autoCompletionApiFiles(), m_autoCompletionEntries(), m_objectAddedToCompletionList(false),
    m_creatorObjects(), m_stringList(), m_unknownTypeObjects(), m_arrayList(), m_tableWidgets(),
    m_tableWidgetObjects(), m_foundUiFiles(), m_parsedFiles()
{
    if(m_autoCompletionApiFiles.isEmpty())
    {
        //Parse all api files.
        QDirIterator it(getScriptEditorFilesFolder()+ "/apiFiles", QStringList() << "*.api", QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext())
        {
            QFile file(it.next());
            if (file.open(QFile::ReadOnly))
            {
                QTextStream in(&file);
                QString singleEntry = in.readLine();
                QString fileName = QFileInfo(file.fileName()).fileName();

                while(!singleEntry.isEmpty())
                {
                    parseSingleLineForFunctionsWithResultObjects(singleEntry);

                    //Add the current line to the api map.
                    m_autoCompletionApiFiles[fileName] << QString(singleEntry).replace("\\n", "\n");
                    singleEntry = in.readLine();
                }
            }
            file.close();
        }
    }
}

/**
 * The thread main function
 */
void ParseThread::run()
{
    QThread::setPriority(QThread::LowestPriority);
    exec();
}

/**
 * Removes in text all left of character.
 * @param text
 *      The string.
 * @param character
 *      The character.
 */
static inline void removeAllLeft(QString& text, QString character)
{
    int index = text.indexOf(character);
    if(index != -1)
    {
        text = text.right((text.length() - index) - 1);
    }
}

/**
 * Searches a single object type.
 * @param className
 *      The class name.
 * @param searchString
 *      The search sring.
 * @param lines
 *      The lines in which shall be searched.
 */
void ParseThread::searchSingleType(QString className, QString searchString, QStringList& lines, bool isArray,
                                    bool withOutDotsAndBracked, bool replaceExistingEntry, bool matchExact)
{
    int index;
    for(int i = 0; i < lines.length();i++)
    {
        index = lines[i].indexOf(searchString);
        if(index != -1)
        {
            if(withOutDotsAndBracked)
            {
                if(lines[i].indexOf(".", index) != -1)
                {
                    continue;
                }
                if(lines[i].indexOf("[", index) != -1)
                {
                    continue;
                }
            }

            if(matchExact)
            {
                if(lines[i].length() != (index + searchString.length()))
                {
                    continue;
                }
            }
            QString objectName =  lines[i].left(index);

            index = lines[i].indexOf("=");
            if(index != -1)
            {
                objectName = lines[i].left(index);
            }

            removeAllLeft(objectName, "{");
            removeAllLeft(objectName, ")");

            bool containsInvalidChars = false;
            for(auto character : objectName)
            {
                if(!character.isLetterOrNumber() && (character != '_') && (character != '[')
                        && (character != ']'))
                {
                    containsInvalidChars = true;
                    break;
                }
            }

            if(containsInvalidChars)
            {
                continue;
            }

            addObjectToAutoCompletionList(objectName, className, false, isArray, replaceExistingEntry);
        }
    }
}

/**
 * Removes all unnecessary characters (e.g. comments).
 * @param currentText
 *      The text.
 */
void ParseThread::removeAllUnnecessaryCharacters(QString& currentText)
{


    //Remove all '/**/' comments.
    QRegExp comment("/\\*(.|[\r\n])*\\*/");
    comment.setMinimal(true);
    comment.setPatternSyntax(QRegExp::RegExp);
    currentText.replace(comment, " ");

    //Remove all '//' comments.
    comment.setMinimal(false);
    comment.setPattern("//[^\n]*");
    currentText.remove(comment);

    currentText.replace("var ", "");
    currentText.replace(" ", "");
    currentText.replace("\t", "");
    currentText.replace("\r", "");
}

/**
 * Removes all square brackets and all between them.
 * @param currentText
 *      The text.
 */
void ParseThread::removeAllBetweenSquareBrackets(QString& currentText)
{
    int index1 = 0;
    int index2 = 0;
    bool textRemoved = true;

    while(textRemoved)
    {
        index1 = currentText.indexOf("[");
        index2 = currentText.indexOf("]", index1);

        if((index1 != -1) && (index2 != -1) && (index2 > index1))
        {
            currentText.remove(index1, (index2 - index1) + 1);
        }
        else
        {
            textRemoved = false;
        }
    }
}
/**
 * Parses a widget list from a user interface file (auto-completion).
 * @param docElem
 *      The QDomElement from the user interface file.
 * @param parseActions
 *      If true then all actions will be parsed. If false then all widgets will be parsed.
 */
void ParseThread::parseWidgetList(QDomElement& docElem, bool parseActions)
{
    QDomNodeList nodeList = docElem.elementsByTagName((parseActions) ? "addaction" : "widget");

    //Parse all widgets or actions.
    for (int i = 0; i < nodeList.size(); i++)
    {
        QDomNode nodeItem = nodeList.at(i);
        QString className = (parseActions) ? "QAction" : nodeItem.attributes().namedItem("class").nodeValue();
        QString objectName = nodeItem.attributes().namedItem("name").nodeValue();

        if(className == QString("QComboBox"))
        {
            className = "ScriptComboBox";
        }
        else if(className == QString("QFontComboBox"))
        {
            className = "ScriptFontComboBox";
        }
        else if(className == QString("QLineEdit"))
        {
            className = "ScriptLineEdit";
        }
        else if(className == QString("QTableWidget"))
        {
            className = "ScriptTableWidget";
        }
        else if(className == QString("QTextEdit"))
        {
            className = "ScriptTextEdit";
        }
        else if(className == QString("QCheckBox"))
        {
            className = "ScriptCheckBox";
        }
        else if(className == QString("QPushButton"))
        {
            className = "ScriptButton";
        }
        else if(className == QString("QToolButton"))
        {
            className = "ScriptToolButton";
        }
        else if(className == QString("QWidget"))
        {
            className = "ScriptWidget";
        }
        else if(className == QString("QDialog"))
        {
            className = "ScriptDialog";
        }
        else if(className == QString("QProgressBar"))
        {
            className = "ScriptProgressBar";
        }
        else if(className == QString("QLabel"))
        {
            className = "ScriptLabel";
        }
        else if(className == QString("QSlider"))
        {
            className = "ScriptSlider";
        }
        else if(className == QString("QDial"))
        {
            className = "ScriptDial";
        }
        else if(className == QString("QMainWindow"))
        {
            className = "ScriptMainWindow";
        }
        else if(className == QString("QAction"))
        {
            className = "ScriptAction";
        }
        else if(className == QString("QTabWidget"))
        {
            className = "ScriptTabWidget";
        }
        else if(className == QString("QGroupBox"))
        {
            className = "ScriptGroupBox";
        }
        else if(className == QString("QRadioButton"))
        {
            className = "ScriptRadioButton";
        }
        else if(className == QString("QSpinBox"))
        {
            className = "ScriptSpinBox";
        }
        else if(className == QString("QDoubleSpinBox"))
        {
            className = "ScriptDoubleSpinBox";
        }
        else if(className == QString("QTimeEdit"))
        {
            className = "ScriptTimeEdit";
        }
        else if(className == QString("QDateEdit"))
        {
            className = "ScriptDateEdit";
        }
        else if(className == QString("QDateTimeEdit"))
        {
            className = "ScriptDateTimeEdit";
        }
        else if(className == QString("QListWidget"))
        {
            className = "ScriptListWidget";
        }
        else if(className == QString("QTreeWidget"))
        {
            className = "ScriptTreeWidget";
        }
        else if(className == QString("QSplitter"))
        {
            className = "ScriptSplitter";
        }
        else if(className == QString("QToolBox"))
        {
            className = "ScriptToolBox";
        }
        else if(className == QString("QCalendarWidget"))
        {
            className = "ScriptCalendarWidget";
        }
        else
        {
            if(!m_autoCompletionApiFiles.contains(className + ".api"))
            {//No API file for the current class.
                className = "";
            }
        }
        addObjectToAutoCompletionList(objectName, className, true);
    }
}


/**
 * Adds on obect to the auto-completion list.
 * @param objectName
 *      The object name.
 * @param className
 *      The class name.
 * @param isGuiElement
 *      True if the object is a GUI element.
 */
void ParseThread::addObjectToAutoCompletionList(QString& objectName, QString& className, bool isGuiElement, bool isArray, bool replaceExistingEntry)
{
    QString autoCompletionName = isGuiElement ? "UI_" + objectName : objectName;


    if(!objectName.isEmpty())
    {

        if(m_autoCompletionEntries.contains(autoCompletionName))
        {
            if(replaceExistingEntry)
            {
                m_autoCompletionEntries.remove(autoCompletionName);
            }
            else
            {
                return;
            }
        }

        if(!isArray)
        {
            if (m_autoCompletionApiFiles.contains(className + ".api"))
            {
                QStringList list = m_autoCompletionApiFiles[className + ".api"];
                m_objectAddedToCompletionList = true;

                for(auto singleEntry : list)
                {
                    singleEntry.replace(className + "::", autoCompletionName + "::");

                    //Add the current line to the api map.
                    m_autoCompletionEntries[autoCompletionName] << singleEntry;
                }
            }
            else
            {
                if(!m_unknownTypeObjects.contains(autoCompletionName))
                {
                    m_unknownTypeObjects.append(autoCompletionName);
                }
            }
        }
        else
        {
            m_objectAddedToCompletionList = true;

            if (m_autoCompletionApiFiles.contains(className + ".api"))
            {
                QStringList list = m_autoCompletionApiFiles[className + ".api"];

                for(auto singleEntry : list)
                {

                    singleEntry.replace(className + "::", autoCompletionName + "[" + "::");

                    //Add the current line to the api map.
                    m_autoCompletionEntries[autoCompletionName + "["] << singleEntry;
                }
            }
            else
            {
                if(!m_unknownTypeObjects.contains(autoCompletionName + "["))
                {
                    m_unknownTypeObjects.append(autoCompletionName + "[");
                }
            }

            QStringList arrayList = m_autoCompletionApiFiles["Array.api"];
            for(auto singleEntry : arrayList)
            {
                singleEntry.replace("Array::", autoCompletionName + "::");

                //Add the current line to the api map.
                m_autoCompletionEntries[autoCompletionName] << singleEntry;

                int index = m_arrayList.indexOf(autoCompletionName);
                if(index != -1)
                {
                    m_arrayList.remove(index);
                }
                m_arrayList.append(autoCompletionName);
            }
        }

        if(className == "String")
        {
            int index = m_stringList.indexOf(autoCompletionName);
            if(index != -1)
            {
                m_stringList.remove(index);
            }
            m_stringList.append(autoCompletionName);
        }
        else if(className == "ScriptTableWidget")
        {
            int index = m_tableWidgets.indexOf(autoCompletionName);
            if(index != -1)
            {
                m_tableWidgets.remove(index);
            }
            m_tableWidgets.append(autoCompletionName);
        }
        else
        {
            if((className != "Dummy"))
            {
                m_creatorObjects[autoCompletionName] = className;
            }
        }
    }
}

/**
 * Parses an user interface file (auto-completion).
 * @param uiFileName
 *      The user interface file.
 */
void ParseThread::parseUiFile(QString uiFileName)
{

    QFile uiFile(uiFileName);
    QDomDocument doc("ui");

    if(!m_parsedFiles.contains(uiFileName))
    {
        m_parsedFiles.append(uiFileName);

        if (uiFile.open(QFile::ReadOnly))
        {
            if (doc.setContent(&uiFile))
            {
                QDomElement docElem = doc.documentElement();
                parseWidgetList(docElem, false);
                parseWidgetList(docElem, true);
            }

            uiFile.close();
            if(!m_foundUiFiles.contains(uiFileName))
            {
                m_foundUiFiles.append(uiFileName);
            }
        }
    }

}
/**
 * Checks if in the current document user interface files are loaded.
 * If user interface are loaded then they will be parsed and added to the auto-completion
 * list (m_autoCompletionEntries).
 */
void ParseThread::checkDocumentForUiFiles(QString& currentText, QString& activeDocument)
{
    int index;

    index = currentText.indexOf("scriptThread.loadUserInterfaceFile");

    while(index != -1)
    {

        QString tmpString =  currentText.right(currentText.length() - index);
        QStringList list = tmpString.split("(");
        QString uiFile;
        bool isRelativePath = true;

        if(list.length() == 1)
        {//( not found.
            break;
        }

        list = list[1].split(")");

        if(list.length() == 1)
        {//) not found.
            break;
        }

        list = list[0].split(",");
        uiFile = list[0];
        if(list.length() > 1)
        {
            isRelativePath = list[1].contains("true") ? true : false;
        }


        uiFile.replace("\"", "");
        uiFile.replace("'", "");

        if(isRelativePath)
        {
            uiFile = QFileInfo(activeDocument).absolutePath() + "/" + uiFile;
        }

        parseUiFile(uiFile);
        index = currentText.indexOf("scriptThread.loadUserInterfaceFile", index + 1);

    }
}

/**
 * Searches all functions which return objects to for a specific object.
 * @param lines
 *      The current lines.
 * @param currentText
 *      The text in which shall be searched.
 * @param functions
 *      The functions for the current object.
 * @param objectName
 *      The name of the current object.
 */
void ParseThread::seachAllFunctionForSpecificObject(QStringList& lines, QString &currentText,
                                                    QVector<FunctionWithResultObject>& functions, QString objectName)
{
    if(currentText.contains("=" + objectName + "."))
    {//The text contains an assignment of a function from the current objet.

        for(auto el : functions)
        {
            searchSingleType(el.resultType, "=" + objectName + "." + el.functionName, lines, el.isArray);
        }
    }
}

/**
 * Searches all dynamically created objects created by custom objects (like ScriptTimer).
 * @param lines
 *      The current lines.
 * @param currentText
 *      The text in which shall be searched.
 */
void ParseThread::checkDocumentForCustomDynamicObjects(QStringList& lines, QStringList &linesWithBrackets , QString &currentText, int passNumber)
{

    if(passNumber == 1)
    {//Static object are only searched in the first pass.

        //Search for the static objects.
        seachAllFunctionForSpecificObject(lines, currentText, m_functionsWithResultObjects["scriptThread"], "scriptThread");
        seachAllFunctionForSpecificObject(lines, currentText, m_functionsWithResultObjects["scriptFile"], "scriptFile");
        seachAllFunctionForSpecificObject(lines, currentText, m_functionsWithResultObjects["conv"], "conv");
        seachAllFunctionForSpecificObject(lines, currentText, m_functionsWithResultObjects["seq"], "seq");
        seachAllFunctionForSpecificObject(lines, currentText, m_functionsWithResultObjects["scriptSql"], "scriptSql");
        seachAllFunctionForSpecificObject(lines, currentText, m_functionsWithResultObjects["cust"], "cust");


        //Search for GUI elements created by a table widget.
        for(auto el : m_tableWidgets)
        {
            parseTableWidetInsert(el, lines);
            seachAllFunctionForSpecificObject(lines, currentText, m_functionsWithResultObjects["ScriptTableWidget"], el);
        }


        //Search for all functions of the GUI elements (created by a table widget) which returns another object.
        QStringList keys = m_tableWidgetObjects.keys();
        for(int i = 0; i < keys.length(); i++)
        {
            searchSingleTableSubWidgets(keys[i], m_tableWidgetObjects.value(keys[i]), lines);
        }

    }//if(passNumber == 1)


    QMap<QString, QString> tmpCreatorList = m_creatorObjects;
    QMap<QString, QString>::iterator i;
    for (i = tmpCreatorList.begin(); i != tmpCreatorList.end(); ++i)
    {
        if(m_functionsWithResultObjects.contains(i.value()))
        {
            //Search for alle functions of the current object wich returns another object.
            seachAllFunctionForSpecificObject(lines, currentText, m_functionsWithResultObjects[i.value()], i.key());
        }

        //Search for assignments of the current object.
        searchSingleType(i.value(), "=" + i.key(), linesWithBrackets, m_arrayList.contains(i.key()), true, false, true);

        if(m_arrayList.contains(i.key()))
        {//Object is an array.

            //Search for assignments of an array element from the current object.
            searchSingleType(i.value(), "=" + i.key() + "[", linesWithBrackets);
        }
    }
}

/**
 * Searches objects which are returned by a ScriptTableWidget.
 * @param objectName
 *      The object name of the ScriptTableWidget.
 * @param subObjects
 *      All objects which have been created by the current ScriptTableWidget (ScriptTableWidget::insertWidget).
 * @param lines
 *      The lines in which shall be searched.
 */
void ParseThread::searchSingleTableSubWidgets(QString objectName, QVector<TableWidgetSubObject> subObjects, QStringList& lines)
{
    int index;
    QString searchString = objectName+ ".getWidget";
    for(int i = 0; i < lines.length();i++)
    {

        index = lines[i].indexOf(searchString);
        if(index != -1)
        {
            QString objectName =  lines[i].left(index);

            index = lines[i].indexOf("=");
            if(index != -1)
            {
                objectName = lines[i].left(index);
            }

            removeAllLeft(objectName, "{");
            removeAllLeft(objectName, ")");

            index = lines[i].indexOf("(", index);
            int endIndex = lines[i].indexOf(")", index);
            if((index != -1) && (endIndex != -1))
            {
                QString args = lines[i].mid(index + 1, endIndex - (index + 1));
                QStringList list = args.split(",");
                if(list.length() >= 2)
                {
                    bool isOk;
                    int row = list[0].toUInt(&isOk);
                    int column = list[1].toUInt(&isOk);

                    //Search the corresponding widget in subObjects.
                    for(int j = 0; j < subObjects.length(); j++)
                    {
                        if((row == subObjects[j].row) && (column == subObjects[j].column))
                        {
                            addObjectToAutoCompletionList(objectName, subObjects[j].className, false);
                        }
                    }
                }
            }
        }
    }
}

/**
 * Searches all ScriptTableWidget::insertWidgets calls of a specific ScriptTableWidget object.
 * @param objectName
 *      The name of the current ScriptTableWidget.
 * @param lines
 *      The lines in which shall be searched.
 */
void ParseThread::parseTableWidetInsert(const QString objectName, QStringList lines)
{
    int index;
    for(int i = 0; i < lines.length();i++)
    {
        QString searchString = objectName + ".insertWidget";

        index = lines[i].indexOf(searchString);
        if(index != -1)
        {

            index = lines[i].indexOf("(", index);
            int endIndex = lines[i].indexOf(")", index);
            if((index != -1) && (endIndex != -1))
            {
                QString args = lines[i].mid(index + 1, endIndex - (index + 1));
                QStringList list = args.split(",");
                if(list.length() >= 3)
                {
                    bool isOk;
                    list[2].replace("\"","");
                    TableWidgetSubObject subObject = {(int)list[0].toUInt(&isOk), (int)list[1].toUInt(&isOk), list[2]};


                    if(subObject.className  == QString("LineEdit"))
                    {
                        subObject.className  = "ScriptLineEdit";
                    }
                    else if(subObject.className == QString("ComboBox"))
                    {
                        subObject.className  = "ScriptComboBox";
                    }
                    else if(subObject.className  == QString("Button"))
                    {
                        subObject.className  = "ScriptButton";
                    }
                    else if(subObject.className  == QString("CheckBox"))
                    {
                        subObject.className  = "ScriptCheckBox";
                    }
                    else if(subObject.className  == QString("SpinBox"))
                    {
                        subObject.className  = "ScriptSpinBox";
                    }
                    else if(subObject.className  == QString("DoubleSpinBox"))
                    {
                        subObject.className  = "ScriptDoubleSpinBox";
                    }
                    else if(subObject.className  == QString("VerticalSlider"))
                    {
                        subObject.className  = "ScriptSlider";
                    }
                    else if(subObject.className  == QString("HorizontalSlider"))
                    {
                        subObject.className  = "ScriptSlider";
                    }
                    else if(subObject.className  == QString("TimeEdit"))
                    {
                        subObject.className  = "ScriptTimeEdit";
                    }
                    else if(subObject.className  == QString("DateEdit"))
                    {
                        subObject.className  = "ScriptDateEdit";
                    }
                    else if(subObject.className  == QString("DateTimeEdit"))
                    {
                        subObject.className  = "ScriptDateTimeEdit";
                    }
                    else if(subObject.className  == QString("CalendarWidget"))
                    {
                        subObject.className  = "ScriptCalendarWidget";
                    }
                    else if(subObject.className  == QString("TextEdit"))
                    {
                        subObject.className  = "ScriptTextEdit";
                    }
                    else if(subObject.className  == QString("Dial"))
                    {
                        subObject.className  = "ScriptDial";
                    }
                    else
                    {
                        subObject.className = "";
                    }

                    if(m_tableWidgetObjects.contains(objectName))
                    {
                        m_tableWidgetObjects[objectName].append(subObject);
                    }
                    else
                    {
                        QVector<TableWidgetSubObject> vect;
                        vect.append(subObject);

                        m_tableWidgetObjects[objectName] = vect;
                    }

                }
            }

        }
    }
}

/**
 * Searches all dynamically created objects created by standard objects (like String).
 * @param currentText
 *      The text in which shall be searched.
 */
void ParseThread::checkDocumentForStandardDynamicObjects(QStringList& lines, QString &currentText, QStringList &linesWithBrackets, int passNumber)
{
    if(passNumber == 1)
    {//Only the first pass the following code shall be executed.

        searchSingleType("Dummy", "=Array(", linesWithBrackets, true);
        searchSingleType("Dummy", "=newArray(", linesWithBrackets, true);

        searchSingleType("Date", "=Date(", linesWithBrackets);
        searchSingleType("Date", "=newDate(", linesWithBrackets);

        searchSingleType("String", "=\"", linesWithBrackets);
        searchSingleType("String", "='", linesWithBrackets);
    }

    QVector<QString> tmpList = m_stringList;
    for(auto el : tmpList)
    {
        seachAllFunctionForSpecificObject(lines, currentText, m_functionsWithResultObjects["String"], el);
        searchSingleType("String", "=" + el, linesWithBrackets, m_arrayList.contains(el), true, false, true);
        searchSingleType("String", "=" + el + "[", linesWithBrackets);


    }

    tmpList = m_arrayList;
    for(auto el : tmpList)
    {
        seachAllFunctionForSpecificObject(lines,currentText,  m_functionsWithResultObjects["Array"], el);
    }

   QMap<QString, QString> tmpCreatorList = m_creatorObjects;
   QMap<QString, QString>::iterator i;
   for (i = tmpCreatorList.begin(); i != tmpCreatorList.end(); ++i)
   {
      if(i.value() == "Date")
       {
           if(m_functionsWithResultObjects.contains(i.value()))
           {
               seachAllFunctionForSpecificObject(lines, currentText, m_functionsWithResultObjects[i.value()], i.key());
           }
       }
   }
}

/**
 * Parses the current text. Emits parsingFinishedSignal if the parsing is finished.
 * @param currentText
 *      The text which shall be parsed.
 * @param activeDocument
 *      The name of the active document.
 */
void ParseThread::parseSlot(QString currentText, QString activeDocument)
{
    //Clear all parsed objects (all but m_autoCompletionApiFiles).
    m_autoCompletionEntries.clear();
    m_creatorObjects.clear();
    m_tableWidgetObjects.clear();
    m_parsedFiles.clear();
    m_stringList.clear();
    m_arrayList.clear();
    m_tableWidgets.clear();
    m_unknownTypeObjects.clear();


    QRegExp splitRegexp("[\n;]");

    //Remove all unnecessary characters (e.g. comments).
    removeAllUnnecessaryCharacters(currentText);

    //Get all lines (with square brackets).
    QStringList linesWithBrackets = currentText.split(splitRegexp);

    //Remove all brackets and all between them.
    removeAllBetweenSquareBrackets(currentText);

    //Get all lines (without square brackets).
    QStringList lines = currentText.split(splitRegexp);

    //Parse all found user interface files (check for changes).
    for(auto el : m_foundUiFiles)
    {
        parseUiFile(el);
    }

    //Check if the active document has a ui-file.
    QString uiFileName = MainWindow::getTheCorrespondingUiFile(activeDocument);
    if(!uiFileName.isEmpty())
    {
        parseUiFile(uiFileName);
    }

    //Find all included user interface files.
    checkDocumentForUiFiles(currentText, activeDocument);

    int counter = 1;
    do
    {
        m_objectAddedToCompletionList = false;

        //Call several times to get all objects created by dynamic objects.
        checkDocumentForCustomDynamicObjects(lines, linesWithBrackets, currentText, counter);
        counter++;
    }while(m_objectAddedToCompletionList);


    counter = 1;
    do
    {
        m_objectAddedToCompletionList = false;

        //Call several times to get all objects created by dynamic objects.
        checkDocumentForStandardDynamicObjects(lines, currentText, linesWithBrackets, counter);
        counter++;
    }while(m_objectAddedToCompletionList);


    //Search all objects with an unknown type.
    searchSingleType("Dummy", "=", lines);


    //Add all objects with an unknown type.
    for(auto el : m_unknownTypeObjects)
    {
        if(!m_autoCompletionEntries.contains(el))
        {
            m_autoCompletionEntries[el] << el;
        }
    }

    emit parsingFinishedSignal(m_autoCompletionEntries, m_autoCompletionApiFiles);
}

