#pragma once

#include <QWidget>

#include <functional>

class QCloseEvent;
class QLabel;
class QTextEdit;
class QTimer;

class WorkNoteEditor final : public QWidget
{
public:
    using SaveHandler = std::function<bool(qint64, const QString &)>;

    WorkNoteEditor(qint64 recordId, const QString &section, const QString &recordedAt,
                   const QString &text, SaveHandler saveHandler, QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    bool saveIfNeeded();

    qint64 m_recordId = 0;
    QTextEdit *m_editor = nullptr;
    QLabel *m_status = nullptr;
    QTimer *m_saveTimer = nullptr;
    SaveHandler m_saveHandler;
    bool m_dirty = false;
};
