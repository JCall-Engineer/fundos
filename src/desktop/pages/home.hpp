#pragma once
#include "main_window.hpp"
#include <QWidget>
#include <QScrollArea>

class HomePage : public QWidget {
	Q_OBJECT

	QScrollArea* make_panel(QWidget* content);

public:
	explicit HomePage(MainWindow* parent = nullptr);
};
