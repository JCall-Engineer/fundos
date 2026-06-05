#include "transaction_dialog.hpp"
using allocated_transaction = fundos::db::transaction_history::allocated_transaction;

TransactionDialog::TransactionDialog(std::shared_ptr<fundos::db> db, allocated_transaction opening, QWidget *parent) : QDialog(parent), database(std::move(db)), record(std::move(opening)) {

}
