#include "transaction_dialog.hpp"
using allocated_transaction = fundos::db::transaction_history::allocated_transaction;

TransactionDialog::TransactionDialog(std::shared_ptr<AppContext> ctx, allocated_transaction opening, QWidget *parent) : QDialog(parent), context(std::move(ctx)), record(std::move(opening)) {

}
