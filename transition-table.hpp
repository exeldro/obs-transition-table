#pragma once

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QGridLayout>
#include <QMainWindow>
#include <QSpinBox>

#include <obs-frontend-api.h>

class TransitionTableDialog : public QDialog {
	Q_OBJECT
	QGridLayout *mainLayout;
	QComboBox *canvasCombo;
	QComboBox *fromCombo;
	QComboBox *toCombo;
	QComboBox *transitionCombo;
	QSpinBox *durationSpin;

	//struct obs_frontend_source_list scenes = {};
	//struct obs_frontend_source_list transitions = {};
	void AddClicked();
	void DeleteClicked();
	void SelectAllChanged();

public:
	TransitionTableDialog(QMainWindow *parent = nullptr);
	~TransitionTableDialog();
public slots:
	void RefreshTable();
	void ShowMatrix();

protected:
	virtual void mouseDoubleClickEvent(QMouseEvent *event) override;
};
