#pragma once
#include "fundos.hpp"
#include <QDialog>
#include <QWidget>

class TransactionDialog : public QDialog {
	Q_OBJECT

	using allocated_transaction = fundos::db::transaction_history::allocated_transaction;
	std::shared_ptr<fundos::db> database;
	allocated_transaction record;

public:
	explicit TransactionDialog(std::shared_ptr<fundos::db> db, allocated_transaction opening, QWidget* parent = nullptr);

signals:
};
