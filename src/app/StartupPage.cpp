#include "app/StartupPage.h"

#include <QFont>
#include <QLabel>
#include <QVBoxLayout>

namespace baludesk::app {

StartupPage::StartupPage(QWidget* parent) : QWidget(parent) {
    auto* heading = new QLabel(tr("Verbindung wird geprüft…"), this);
    QFont headingFont = heading->font();
    headingFont.setPointSize(headingFont.pointSize() + 6);
    heading->setFont(headingFont);

    auto* explanation = new QLabel(
        tr("Es wird nachgesehen, ob eine Sitzung abgelegt ist und ob der Server "
           "antwortet. Schläft der Server, kann das eine halbe Minute dauern."),
        this);
    explanation->setWordWrap(true);

    auto* layout = new QVBoxLayout(this);
    layout->addStretch(1);
    layout->addWidget(heading);
    layout->addSpacing(12);
    layout->addWidget(explanation);
    layout->addStretch(2);
}

}  // namespace baludesk::app
