#include "data/import.hpp"
#include "import_dialog.hpp"

ImportDialog::ImportDialog(std::shared_ptr<AppContext> ctx, QWidget *parent) : QDialog(parent), context(std::move(ctx)) {

}
