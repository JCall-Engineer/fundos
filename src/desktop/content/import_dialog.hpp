#pragma once
#include "context.hpp"
#include <QDialog>
#include <QWidget>

class ImportDialog : public QDialog {
	Q_OBJECT

	std::shared_ptr<AppContext> context;

public:
	explicit ImportDialog(std::shared_ptr<AppContext> ctx, QWidget* parent = nullptr);

signals:
};
