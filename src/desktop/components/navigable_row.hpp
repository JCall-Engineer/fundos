#pragma once
#include <QLabel>
#include <QWidget>

class NavigableRow : public QWidget {
	Q_OBJECT
public:
	explicit NavigableRow(int index, const QString& name, QLabel* amount_label, QWidget* parent = nullptr);

signals:
	void clicked(int index);
};
