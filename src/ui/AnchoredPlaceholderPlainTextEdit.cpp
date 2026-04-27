#include "ui/AnchoredPlaceholderPlainTextEdit.h"

#include <QPainter>
#include <QPaintEvent>
#include <QTextOption>

namespace DentalPatients {

AnchoredPlaceholderPlainTextEdit::AnchoredPlaceholderPlainTextEdit(QWidget* parent)
    : QPlainTextEdit(parent) {
    connect(this, &QPlainTextEdit::textChanged, viewport(), qOverload<>(&QWidget::update));
}

AnchoredPlaceholderPlainTextEdit::AnchoredPlaceholderPlainTextEdit(const QString& text, QWidget* parent)
    : QPlainTextEdit(text, parent) {
    connect(this, &QPlainTextEdit::textChanged, viewport(), qOverload<>(&QWidget::update));
}

void AnchoredPlaceholderPlainTextEdit::setAnchoredPlaceholderText(const QString& text) {
    m_placeholderText = text;
    QPlainTextEdit::setPlaceholderText(QString());
    viewport()->update();
}

void AnchoredPlaceholderPlainTextEdit::paintEvent(QPaintEvent* event) {
    QPlainTextEdit::paintEvent(event);

    if (!document()->isEmpty() || m_placeholderText.isEmpty()) {
        return;
    }

    const QTextOption defaultOption = document()->defaultTextOption();
    const bool rtl = defaultOption.textDirection() == Qt::RightToLeft
                  || layoutDirection() == Qt::RightToLeft;

    QRect textRect = viewport()->rect();
    const QRect caret = cursorRect();
    textRect.setTop(caret.top());
    if (rtl) {
        textRect.setRight(caret.left());
    } else {
        textRect.setLeft(caret.right());
    }

    QTextOption option = defaultOption;
    option.setTextDirection(rtl ? Qt::RightToLeft : Qt::LeftToRight);
    option.setAlignment((rtl ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignAbsolute);

    QPainter painter(viewport());
    painter.setPen(palette().color(QPalette::PlaceholderText));
    painter.drawText(textRect, m_placeholderText, option);
}

} // namespace DentalPatients
