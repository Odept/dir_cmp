#pragma once

#include <QMainWindow>

#include "treemodel.h"


namespace Ui
{
	class MainWindow;
}


class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget* parent = nullptr);
	~MainWindow();

private:
	void selectAndSetDir(class QPlainTextEdit& _edit_ptr);

private slots:
	void onBrowseBaselinePressed();
	void onBrowseTargetPressed();

private:
	Ui::MainWindow* m_ui;

	std::unique_ptr<TreeModel> m_treeModel;
};
