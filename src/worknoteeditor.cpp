#include "worknoteeditor.h"

#include <QCloseEvent>
#include <QDateTime>
#include <QFrame>
#include <QFont>
#include <QLabel>
#include <QScrollBar>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

#include <utility>

WorkNoteEditor::WorkNoteEditor(qint64 recordId, const QString &section,
                               const QString &recordedAt, const QString &text,
                               SaveHandler saveHandler, QWidget *parent)
    : QWidget(parent, Qt::Window)
    , m_recordId(recordId)
    , m_saveHandler(std::move(saveHandler))
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(QStringLiteral("work · %1 · %2").arg(section, recordedAt));
    resize(760, 520);
    setMinimumSize(520, 360);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(18, 16, 18, 16);
    root->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("%1  ·  %2").arg(section, recordedAt));
    QFont titleFont = title->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    title->setFont(titleFont);
    root->addWidget(title);

    auto *separator = new QFrame;
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Plain);
    root->addWidget(separator);

    m_editor = new QTextEdit;
    m_editor->setAcceptRichText(false);
    m_editor->setLineWrapMode(QTextEdit::WidgetWidth);
    m_editor->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_editor->setPlainText(text);
    m_editor->setPlaceholderText(QStringLiteral("在这里记录或修改详细内容……"));
    root->addWidget(m_editor, 1);

    m_status = new QLabel(QStringLiteral("已加载 · 每 1 秒自动保存"));
    m_status->setStyleSheet(QStringLiteral("color: #4b5563;"));
    root->addWidget(m_status);

    m_saveTimer = new QTimer(this);
    m_saveTimer->setInterval(1000);
    m_saveTimer->start();

    connect(m_editor, &QTextEdit::textChanged, this, [this] {
        m_dirty = true;
        m_status->setText(QStringLiteral("等待自动保存……"));
    });
    connect(m_saveTimer, &QTimer::timeout, this, [this] { saveIfNeeded(); });

    m_editor->setFocus();
}

void WorkNoteEditor::closeEvent(QCloseEvent *event)
{
    if (!saveIfNeeded()) {
        event->ignore();
        return;
    }
    QWidget::closeEvent(event);
}

bool WorkNoteEditor::saveIfNeeded()
{
    if (!m_dirty)
        return true;

    const QString text = m_editor->toPlainText();
    if (!m_saveHandler || !m_saveHandler(m_recordId, text)) {
        m_status->setText(QStringLiteral("自动保存失败，将在 1 秒后重试"));
        m_status->setStyleSheet(QStringLiteral("color: #b91c1c; font-weight: 600;"));
        return false;
    }

    m_dirty = false;
    m_status->setText(QStringLiteral("已自动保存  %1")
                          .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))));
    m_status->setStyleSheet(QStringLiteral("color: #166534;"));
    return true;
}
