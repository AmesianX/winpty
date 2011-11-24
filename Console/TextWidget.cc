#include "TextWidget.h"
#include "../Shared/AgentClient.h"
#include "../Shared/AgentMsg.h"
#include <QKeyEvent>
#include <QtDebug>
#include <windows.h>

TextWidget::TextWidget(QWidget *parent) :
    QWidget(parent),
    m_agentClient(NULL)
{
}

void TextWidget::initWithAgent(AgentClient *agentClient)
{
    m_agentClient = agentClient;
}

void TextWidget::keyPressEvent(QKeyEvent *event)
{
    // TODO: Flesh this out....  Also: this code is intended
    // to be portable across operating systems, so using
    // nativeVirtualKey is wrong.
    if (event->nativeVirtualKey() != 0) {
        AgentMsg msg;
        memset(&msg, 0, sizeof(msg));
        msg.type = AgentMsg::InputRecord;
        INPUT_RECORD &ir = msg.u.inputRecord;
        ir.EventType = KEY_EVENT;
        ir.Event.KeyEvent.bKeyDown = TRUE;
        ir.Event.KeyEvent.wVirtualKeyCode = event->nativeVirtualKey();
        qDebug() << ir.Event.KeyEvent.wVirtualKeyCode;
        ir.Event.KeyEvent.wVirtualScanCode = 0; // event->nativeScanCode();
        ir.Event.KeyEvent.uChar.UnicodeChar =
                event->text().isEmpty() ? L'\0' : event->text().at(0).unicode();
        ir.Event.KeyEvent.wRepeatCount = event->count();
        m_agentClient->writeMsg(msg);
    }
}

void TextWidget::keyReleaseEvent(QKeyEvent *event)
{
    //m_agentClient->sendKeyRelease(event);
}
