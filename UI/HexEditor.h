#pragma once

#include <QWidget>
#include <vector>

class QAbstractTableModel;
class QLineEdit;
class QPushButton;
class QTableView;

class HexEditor : public QWidget {
    Q_OBJECT

public:
    explicit HexEditor(QWidget* parent = nullptr);

    void setBytes(const std::vector<unsigned char>& bytes);
    void retranslate(bool russian);

    [[nodiscard]] bool isModified() const noexcept { return modified_; }

public slots:
    bool saveAs();

private slots:
    void goToOffset();
    void bytesChanged();

private:
    std::vector<unsigned char> bytes_;
    QAbstractTableModel* hexModel_ { nullptr };
    QAbstractTableModel* asciiModel_ { nullptr };
    QTableView* hexView_ { nullptr };
    QTableView* asciiView_ { nullptr };
    QLineEdit* offsetEdit_ { nullptr };
    QPushButton* goButton_ { nullptr };
    QPushButton* saveButton_ { nullptr };
    bool modified_ { false };
    bool syncing_ { false };
    bool russian_ { false };
};
