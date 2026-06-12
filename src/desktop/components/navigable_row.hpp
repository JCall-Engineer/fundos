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

	explicit NavigableRow(const props& data, const QString& name, QWidget* parent = nullptr);

private:
	props properties;

public slots:
	void on_toggle(bool);
	void set_amount(QLabel* amount_label, bool has_amount);

signals:
	void clicked(size_t index);
};
