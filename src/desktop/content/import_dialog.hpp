#pragma once
#include "fundos.hpp"
#include <QDialog>
#include <QWidget>

class ImportDialog : public QDialog {
	Q_OBJECT

	std::shared_ptr<fundos::db> database;

public:
	explicit ImportDialog(std::shared_ptr<fundos::db> db, QWidget* parent = nullptr);

signals:
};
