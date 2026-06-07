#pragma once
#include <QLabel>
#include <QWidget>

class NavigableRow : public QWidget {
	Q_OBJECT

public:
	struct props {
		size_t index;
		bool is_closed;
		bool has_amount = false;
	};

	explicit NavigableRow(const props& data, const QString& name, QLabel* amount_label, QWidget* parent = nullptr);

private:
	props properties;

public slots:
	void on_toggle(bool);

signals:
	void clicked(size_t index);
};
