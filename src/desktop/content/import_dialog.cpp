#include "data/import.hpp"
#include "import_dialog.hpp"

ImportDialog::ImportDialog(std::shared_ptr<fundos::db> db, QWidget *parent) : QDialog(parent), database(std::move(db)) {

}
