#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <QStandardPaths>
#include <QScrollBar>


void MainWindow::selectAndSetDir(QPlainTextEdit& _edit_ptr)
{
	auto locations = QStandardPaths::standardLocations(QStandardPaths::HomeLocation);
	QString dirPath = QFileDialog::getExistingDirectory(this, "Open Baseline Directory", locations.first());
	_edit_ptr.setPlainText(dirPath);
}

void MainWindow::onBrowseBaselinePressed()
{
	selectAndSetDir(*m_ui->editBaseline);
}

void MainWindow::onBrowseTargetPressed()
{
	selectAndSetDir(*m_ui->editTarget);
}


MainWindow::MainWindow(QWidget* parent):
	QMainWindow{parent},
	m_ui{new Ui::MainWindow},
	m_treeModel{std::make_unique<TreeModel>()}
{
	m_ui->setupUi(this);
	m_ui->treeViewCompare->setModel(m_treeModel.get());

	QFontMetrics fm(m_ui->treeViewCompare->header()->font());
	int minColumnWidth = fm.horizontalAdvance("100") + qApp->style()->pixelMetric(QStyle::PM_ScrollBarExtent);
	m_ui->treeViewCompare->header()->setMinimumSectionSize(minColumnWidth * 4 / 3);
	m_ui->treeViewCompare->header()->setSectionResizeMode(0, QHeaderView::ResizeMode::Stretch);
	m_ui->treeViewCompare->header()->setSectionResizeMode(1, QHeaderView::ResizeMode::ResizeToContents);
}

MainWindow::~MainWindow()
{
	delete m_ui;
}
