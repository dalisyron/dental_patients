# Design Guidelines

These guidelines capture the current UI decisions for Dental Patients. Future
features should follow them unless there is a deliberate product reason to do
otherwise.

## Editable Text Fields

The application is Persian-first and right-to-left by default, but field
direction must follow the meaning of the field, not only the application
language.

Do not rely on the application or dialog layout direction alone for editable
controls. Text inputs need explicit per-field direction, alignment, and cursor
movement settings so placeholder text, typed text, and caret placement stay
consistent.

### Field Semantics

- Persian free-text fields are RTL and right aligned. This includes search,
  family name, given name, phone, and notes.
- Numeric identifier fields that are entered with English digits are LTR. The
  current case-number field intentionally keeps this behavior.
- If a field is semantically Persian/free text but may contain occasional
  English digits or Latin words, keep the field RTL. Normalize stored values
  separately when needed.
- Use visible labels for important form fields. Placeholder text is only a
  short hint or example and must not replace a label.

### Caret And Placeholder Anchor

There is no fixed pixel gap between the caret and placeholder text.

The caret belongs at the exact insertion point where the first typed character
will appear. In an empty field, that is text position `0`.

The field padding defines the shared text-start anchor for both the caret and
the placeholder. Do not add a separate offset between them.

For LTR fields:

```text
|Example
```

For RTL Persian fields, the same rule applies on the right side:

```text
        |جستجو کنید
```

The placeholder is not real text. It is painted while the value is empty, so it
should visually begin from the same anchor where typed text will begin. When the
user types the first character, the text should appear at that same position
without the field jumping or changing alignment.

### Qt Implementation

For Persian `QLineEdit` fields, match the search field behavior:

```cpp
field->setLayoutDirection(Qt::RightToLeft);
field->setAlignment(Qt::AlignRight | Qt::AlignAbsolute | Qt::AlignVCenter);
field->setCursorMoveStyle(Qt::LogicalMoveStyle);
```

For Persian `QPlainTextEdit` fields, set both the widget direction and the
document/block direction. Use `AnchoredPlaceholderPlainTextEdit` when a
multiline field needs placeholder text; its placeholder is painted from the
empty cursor anchor so the caret does not appear after the first placeholder
character in RTL fields.

```cpp
field->setLayoutDirection(Qt::RightToLeft);

QTextOption option = field->document()->defaultTextOption();
option.setTextDirection(Qt::RightToLeft);
option.setAlignment(Qt::AlignRight | Qt::AlignAbsolute);
field->document()->setDefaultTextOption(option);
field->document()->setDefaultCursorMoveStyle(Qt::LogicalMoveStyle);

QTextBlockFormat format;
format.setLayoutDirection(Qt::RightToLeft);
format.setAlignment(Qt::AlignRight | Qt::AlignAbsolute);

QTextCursor cursor(field->document());
cursor.select(QTextCursor::Document);
cursor.mergeBlockFormat(format);
```

For new numeric identifier fields, use explicit LTR behavior if the global RTL
layout causes ambiguous caret placement:

```cpp
field->setLayoutDirection(Qt::LeftToRight);
field->setAlignment(Qt::AlignLeft | Qt::AlignAbsolute | Qt::AlignVCenter);
```

### Placeholder Text

- Keep placeholders short and concrete, such as an example value or "optional".
- Match the placeholder to the expected script and direction of the field.
- Do not use placeholder text for validation rules that the user must remember
  after typing. Put that information in a visible label, helper text, or error
  message.
- Required fields should have visible labels and validation feedback, not only a
  placeholder.

### Regression Checklist

Before shipping changes to editable text fields:

- Select an empty field and confirm the caret appears at text position `0`.
- Confirm the placeholder and first typed character share the same visual
  start anchor.
- Confirm Persian text starts on the right in Persian/free-text fields.
- Confirm English-digit numeric identifiers start on the left in numeric fields.
- Confirm arrow-key movement feels logical in mixed Persian/English content.
- Confirm existing values and placeholders use the same alignment.
- Confirm multiline fields keep the same alignment when adding new lines.
