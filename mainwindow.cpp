#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    model(new TeamMemberListModel(this)),
    memberName(),
    db_file("database.xml"),
    reg(),
    readingBarcode(false)
{
    ui->setupUi(this);

    connect(ui->actionNew_2, SIGNAL(triggered(bool)), this, SLOT(newData()));
    connect(ui->actionOpen_2, SIGNAL(triggered(bool)), this, SLOT(openData()));
    connect(ui->actionSave_2, SIGNAL(triggered(bool)), this, SLOT(saveData()));
    connect(ui->actionExport, SIGNAL(triggered(bool)), this, SLOT(exportData()));
    connect(ui->actionRegister, SIGNAL(triggered(bool)), this, SLOT(beginRegister()));
    connect(ui->actionRegister, SIGNAL(triggered(bool)), &reg, SLOT(show()));
    connect(ui->barcodeInput, SIGNAL(textChanged(QString)), this, SLOT(startBarcodeRead()));

    connect(&reg, SIGNAL(registered(TeamMember)), this, SLOT(finishRegister(TeamMember)));

    ui->nameList->setModel(model);
    ui->nameList->setEditTriggers(QAbstractItemView::NoEditTriggers);

    //Populate model with names

    if (db_file.open(QFile::ReadWrite | QFile::Text))
    {
        using namespace rapidxml;
        QTextStream in(&db_file);

        db_text = db.allocate_string("", db_file.size());

        strcpy(db_text, in.readAll().toStdString().c_str());

        // Use parse_no_data_nodes so that the data can be edited later
        db.parse<parse_no_data_nodes>(db_text);
        xml_node<> *root_node = db.first_node();
        xml_node<> *team_members_node = root_node->first_node("teammembers");

        nextUid = atoi(team_members_node->first_node("uid")->value());

        QVariant var;
        int i = 0;
        for (xml_node<> *member_data = team_members_node->first_node(); member_data; member_data = member_data->next_sibling())
            i++;

        model->insertRows(0, i);

        i = 0;
        for (xml_node<> *member_data = team_members_node->first_node("member"); member_data; member_data = member_data->next_sibling(), i++)
        {
            char* name = db.allocate_string("");
            strcat(name, member_data->first_attribute("fname")->value());
            strcat(name, " ");
            strcat(name, member_data->first_attribute("lname")->value());

            TeamMember temp(name, atoi(member_data->first_attribute("uid")->value()));
            temp.email = member_data->first_attribute("eml")->value();
            temp.parentEmail = member_data->first_attribute("peml")->value();
            temp.subteam = member_data->first_attribute("team")->value();
            temp.grade = atoi(member_data->first_attribute("grade")->value());

            var.setValue(temp);
            model->setData(model->index(i), var);
        }
    }
    else
    {
        //TODO handle error
    }
    connect(ui->nameList, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(openMemberView(QModelIndex)));

    //Initialize Member View UI

    ui->memberView->hide();

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    char* nextUidString = db.allocate_string("", 8);
    sprintf(nextUidString, "%d", nextUid);
    db.first_node()->first_node("teammembers")->first_node("uid")->value(nextUidString);

    std::string s;
    rapidxml::print(std::back_inserter(s), db);

    db_file.resize(0);
    db_file.write(s.c_str());
}

void MainWindow::newData()
{
    qDebug() << "new";
}

void MainWindow::openData()
{
    qDebug() << "open";
}

void MainWindow::saveData()
{
    qDebug() << "save";
}

void MainWindow::exportData()
{
    using namespace rapidxml;

    xml_document<> doc;
    QFile templateFile ("template.html");
    if(templateFile.open(QFile::ReadWrite | QFile::Text))
    {
        QTextStream in(&templateFile);

        char* templateText = db.allocate_string("", db_file.size());
        strcpy(templateText, in.readAll().toStdString().c_str());
        doc.parse<0>(templateText);
    }
    else
        return;

    xml_node<> *body = doc.first_node("body");

    QString nametags("<table><tbody>");
    // Populate the table with nametags

    for(QList<TeamMember>::iterator member = model->memberList.begin(); member != model->memberList.end(); member++)
    {
        //Start row
        nametags.append("<tr><td class=\"name\">");

        //First Column
        nametags.append(member->name);
        nametags.append("<br/>");
        nametags.append(member->subteam);
        nametags.append("</td><td class=\"barcode\">");
        nametags.append(QString("*%1*").arg(member->uid, 8, 10, QChar('0')));
        nametags.append("</td><td class=\"name\">");

        //Second Column
        member++;
        if(member == model->memberList.end())
            break;
        nametags.append(member->name);
        nametags.append("<br/>");
        nametags.append(member->subteam);
        nametags.append("</td><td class=\"barcode\">");
        nametags.append(QString("*%1*").arg(member->uid, 8, 10, QChar('0')));
        nametags.append("</td>");

        nametags.append("</tr>");
        //End row
    }
    // End the table
    nametags.append("</tbody></table>");

    qDebug() << nametags;

    rapidxml::xml_document<> tableData;
    char* parse_data = doc.allocate_string(nametags.toStdString().c_str());
    tableData.parse<0>(parse_data);
    rapidxml::xml_node<> *clone = doc.clone_node(tableData.first_node());
    doc.first_node()->first_node()->append_node(clone);

    std::string s;
    rapidxml::print(std::back_inserter(s), doc);
    QString str(s.c_str());
    str.replace("&quot;", "\"");
    s = str.toStdString();
    QString exportFileName = QFileDialog::getSaveFileName(this, QString(), QString(), tr("HTML Files (*.html)"));
    QFile exportFile (exportFileName);
    if(exportFile.open(QFile::ReadWrite | QFile::Text))
    {
        exportFile.resize(0);
        exportFile.write(s.c_str());
    }
}

void MainWindow::beginRegister()
{

}
void MainWindow::finishRegister(TeamMember member)
{
    //Assign the new member a uid
    member.uid = nextUid;
    nextUid++;

    // Put new member into database
    rapidxml::xml_document<> member_data;
    char* parse_data = db.allocate_string(member.getXML().toStdString().c_str());
    member_data.parse<0>(parse_data);
    rapidxml::xml_node<> *clone = db.clone_node(member_data.first_node());
    db.first_node()->first_node()->append_node(clone);

    //Display new member in member list
    QVariant temp;
    temp.setValue(member);
    model->setData(model->index(model->rowCount()-1), temp);
    model->refresh();
}
void MainWindow::openMemberView(QModelIndex index)
{
    TeamMember member = qvariant_cast<TeamMember>(model->data(index, 6));

    ui->name->setText(member.name);
    ui->grade->setText(((member.grade == 13) ? "Mentor":QString("%1th Grade").arg(member.grade)));
    ui->team->setText(member.subteam);
    ui->email->setText(member.email);
    ui->parentEmail->setText(member.parentEmail);

    ui->memberView->show();
}


void MainWindow::startBarcodeRead()
{
    if(!readingBarcode)
    {
        //Give the reader time to write all the characters of the barcode
        //If this is regular keyboard input, it will take longer than 100 ms, and then be cleared out
        QTimer::singleShot(100, this, SLOT(endBarcodeRead()));
        readingBarcode = true;
    }
}

void MainWindow::endBarcodeRead()
{
    if(QRegExp("^[0-9]{1,8}$").exactMatch(ui->barcodeInput->text()))
    {
        int searchUid = ui->barcodeInput->text().toInt();
        //Input matches valid barcode format
        int i = 0;
        for(QList<TeamMember>::iterator it = model->memberList.begin(); it != model->memberList.end(); it++, i++)
        {
            if(it->uid == searchUid)
            {
                openMemberView(model->index(i));
            }
        }
    }
    ui->barcodeInput->setText(QString(""));
    readingBarcode = false;
}
