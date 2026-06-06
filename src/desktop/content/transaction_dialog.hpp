#pragma once
#include "context.hpp"
#include <QDialog>
#include <QWidget>

class TransactionDialog : public QDialog {
	Q_OBJECT

	using allocated_transaction = fundos::db::transaction_history::allocated_transaction;
	std::shared_ptr<AppContext> context;
	allocated_transaction record;

public:
	explicit TransactionDialog(std::shared_ptr<AppContext> ctx, allocated_transaction opening, QWidget* parent = nullptr);

signals:
};
