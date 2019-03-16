/*
Copyright 2017-2018 Mikhail Milovidov <milovidovmikhail@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "Precompiled.h"
#include "CApplicationSettings.h"
#include <DaggyCore/CDataSourcesFabric.h>

using namespace daggycore;

CApplicationSettings::CApplicationSettings()
{
#ifdef Q_OS_UNIX
    qApp->setApplicationVersion(QString("%1.%2").arg(DAGGY_VERSION).arg(DAGGY_BUILD_NUMBER));
#endif
    qApp->setApplicationName(PROGRAM_NAME);

    CDataSourcesFabric data_sources_fabric;

    QCommandLineParser command_line_parser;
    command_line_parser.addVersionOption();
    const QCommandLineOption output_folder_option({"o", "output"}, "Set output folder", "folder", "");
    const QCommandLineOption input_format_option({"f", "format"}, "Source format", "format", data_sources_fabric.inputTypeName(CDataSourcesFabric::json));

    command_line_parser.addOption(output_folder_option);
    command_line_parser.addOption(input_format_option);

    command_line_parser.setApplicationDescription("Data agregation utility is a program for extracting information from multiple remote servers via ssh simultaneously.");
    command_line_parser.addHelpOption();

    command_line_parser.addPositionalArgument("sources", "Data sources", "sourceFile");

    command_line_parser.process(*qApp);

    QString data_source_type = command_line_parser.value(input_format_option);

    QString data_sources_text;
    QString data_source_name("stdin");
    const QStringList& positional_arguments = command_line_parser.positionalArguments();
    const QString& source_file_name = positional_arguments[0];
    if (!source_file_name.isEmpty()) {
        const QString& source_file_name = positional_arguments[0];
        data_source_name = QFileInfo(source_file_name).baseName();
        if (!command_line_parser.isSet(input_format_option))
            data_source_type = QFileInfo(source_file_name).suffix();
    }

    if (!data_sources_fabric.isSourceTypeSopported(data_source_type))
        qFatal("Invalid source format: %s. Supported formats: [%s]", qPrintable(data_source_type), qPrintable(data_sources_fabric.supportedSourceTypes().join(", ")));

    if (positional_arguments.isEmpty()) {
        data_sources_text = QTextStream(stdin).readAll();
    } else {
        data_sources_text = getTextFromFile(source_file_name);
    }

    data_sources_ = data_sources_fabric.getDataSources(data_sources_fabric.sourceTypeValue(data_source_type), data_sources_text);
    output_folder_ = command_line_parser.value(output_folder_option);

    if (output_folder_.isEmpty())
        output_folder_ = getOutputFolderPath(data_source_name);
}

const QString& CApplicationSettings::outputFolder() const
{
    return output_folder_;
}

const DataSources& CApplicationSettings::dataSources() const
{
    return data_sources_;
}

QString CApplicationSettings::getOutputFolderPath(const QString& data_source_name) const
{
    const QString& current_date = QDateTime::currentDateTime().toString("dd-MM-yy_hh-mm-ss");
    QString result = current_date + "-" + data_source_name;
    return result;
}

QString CApplicationSettings::getTextFromFile(QString file_path) const
{
    QString result;
    if (!QFileInfo(file_path).exists()) {
        file_path = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.daggy/" + file_path;
    }

    QFile source_file(file_path);
    if (source_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result = source_file.readAll();
    } else {
        qFatal("Cann't open %s file for read: %s", qPrintable(file_path), qPrintable(source_file.errorString()));
    }

    return result;
}