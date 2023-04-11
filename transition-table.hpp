#pragma once

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QMainWindow>
#include <QDialog>
#include <QGridLayout>
#include <QSpinBox>

#include <obs-frontend-api.h>

class TransitionTableDialog : public QDialog {
	Q_OBJECT
	QGridLayout *mainLayout;
	QGridLayout *newLayout;
	QComboBox *fromCombo;
	QComboBox *toCombo;
	QComboBox *transitionCombo;
	QSpinBox *durationSpin;
	QCheckBox *ignoreDefaultCheckBox;
	QComboBox *selectSetCombo;

	struct obs_frontend_source_list scenes = {};
	struct obs_frontend_source_list transitions = {};
	void AddClicked();
	void DeleteClicked();
	void SelectAllChanged();
	void DuplicateSetClicked();
	void NewSetClicked();
	void RenameSetClicked();
	void DeleteSetClicked();
	void SetDefaultSetClicked();
	void IgnoreDefaultChanged();
	void SelectSetChanged();

public:
	TransitionTableDialog(QMainWindow *parent = nullptr);
	~TransitionTableDialog();
public slots:
	void RefreshTable();
	void ShowMatrix();
	void RefreshMatrix();

protected:
	virtual void mouseDoubleClickEvent(QMouseEvent *event) override;
};


class TransitionTableRefresh : public QObject {
	Q_OBJECT
public:
	void signalRefresh();
signals:
	void sigRefresh();
};
