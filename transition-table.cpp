
#include "transition-table.hpp"
#include "version.h"

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QCheckBox>
#include <QComboBox>

#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QTableView>
#include <QtWidgets/QColorDialog>

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("Exeldro");
OBS_MODULE_USE_DEFAULT_LOCALE("transition-table", "en-US")

using namespace std;

struct transition_info {
	string transition;
	int duration;
};

map<string, map<string, transition_info>> transition_table;

static void frontend_save_load(obs_data_t *save_data, bool saving, void *)
{
	if (saving) {
		obs_data_t *obj = obs_data_create();
		obs_data_array_t *transitions = obs_data_array_create();
		obs_data_set_obj(save_data, "transition-table", obj);
		for (const auto &it : transition_table) {
			for (const auto &it2 : it.second) {
				obs_data_t *transition = obs_data_create();
				obs_data_set_string(transition, "from_scene",
						    it.first.c_str());
				obs_data_set_string(transition, "to_scene",
						    it2.first.c_str());
				obs_data_set_string(
					transition, "transition",
					it2.second.transition.c_str());
				obs_data_set_int(transition, "duration",
						 it2.second.duration);
				obs_data_array_push_back(transitions,
							 transition);
			}
		}
		obs_data_set_array(obj, "transitions", transitions);
		obs_data_set_obj(save_data, "transition-table", obj);
		obs_data_array_release(transitions);
		obs_data_release(obj);
	} else {
		transition_table.clear();
		obs_data_t *obj =
			obs_data_get_obj(save_data, "transition-table");
		obs_data_array_t *transitions =
			obs_data_get_array(obj, "transitions");
		if (transitions) {
			size_t count = obs_data_array_count(transitions);
			for (size_t i = 0; i < count; i++) {
				obs_data_t *transition =
					obs_data_array_item(transitions, i);
				string fromScene = obs_data_get_string(
					transition, "from_scene");
				string toScene = obs_data_get_string(
					transition, "to_scene");
				string transitionName = obs_data_get_string(
					transition, "transition");
				bool custom_duration = obs_data_get_bool(
					transition, "custom_duration");
				const uint32_t duration = obs_data_get_int(
					transition, "duration");
				transition_table[fromScene][toScene].transition =
					transitionName;
				transition_table[fromScene][toScene].duration =
					duration;
			}
			obs_data_array_release(transitions);
		}
		obs_data_release(obj);
	}
}

static void frontend_event(enum obs_frontend_event event, void *)
{
	if (event != OBS_FRONTEND_EVENT_SCENE_CHANGED)
		return;

	obs_source_t *scene = obs_frontend_get_current_scene();
	string fromScene = obs_source_get_name(scene);
	obs_source_release(scene);

	auto fs_it = transition_table.find(fromScene);
	auto as_it = transition_table.find("Any");

	obs_frontend_source_list scenes = {};
	obs_frontend_get_scenes(&scenes);
	for (size_t i = 0; i < scenes.sources.num; i++) {
		string toScene = obs_source_get_name(scenes.sources.array[i]);
		string transition;
		auto duration = 0;
		if (fs_it != transition_table.end()) {
			auto to_it = fs_it->second.find(toScene);
			if (to_it == fs_it->second.end()) {
				to_it = fs_it->second.find("Any");
			}
			if (to_it != fs_it->second.end()) {
				transition = to_it->second.transition;
				duration = to_it->second.duration;
			}
		}
		if (transition.empty() && as_it != transition_table.end()) {
			auto to_it = as_it->second.find(toScene);
			if (to_it == as_it->second.end()) {
				to_it = as_it->second.find("Any");
			}
			if (to_it != as_it->second.end()) {
				transition = to_it->second.transition;
				duration = to_it->second.duration;
			}
		}
		obs_data_t *data = obs_source_get_private_settings(
			scenes.sources.array[i]);
		if (transition.empty()) {
			obs_data_erase(data, "transition");
		} else {
			obs_data_set_string(data, "transition",
					    transition.c_str());
			obs_data_set_int(data, "transition_duration", duration);
		}
		obs_data_release(data);
	}
	obs_frontend_source_list_free(&scenes);
}

bool obs_module_load(void)
{
	blog(LOG_INFO, "[Transition Table] loaded version %s", PROJECT_VERSION);
	auto *action = (QAction *)obs_frontend_add_tools_menu_qaction(
		obs_module_text("TransitionTable"));

	auto cb = [] {
		obs_frontend_push_ui_translation(obs_module_get_string);

		TransitionTableDialog ttd(
			(QMainWindow *)obs_frontend_get_main_window());
		ttd.exec();

		obs_frontend_pop_ui_translation();
	};

	QAction::connect(action, &QAction::triggered, cb);

	obs_frontend_add_save_callback(frontend_save_load, nullptr);
	obs_frontend_add_event_callback(frontend_event, nullptr);

	return true;
}

void obs_module_unload(void) {}

MODULE_EXPORT const char *obs_module_description(void)
{
	return obs_module_text("Description");
}

MODULE_EXPORT const char *obs_module_name(void)
{
	return obs_module_text("TransitionTable");
}

TransitionTableDialog::TransitionTableDialog(QMainWindow *parent)
	: QDialog(parent)
{
	obs_frontend_get_scenes(&scenes);
	obs_frontend_get_transitions(&transitions);

	int idx = 0;
	mainLayout = new QGridLayout;
	mainLayout->setContentsMargins(0, 0, 0, 0);
	QLabel *label = new QLabel(obs_module_text("FromScene"));
	label->setStyleSheet("font-weight: bold;");
	mainLayout->addWidget(label, 0, idx++, Qt::AlignCenter);
	label = new QLabel(obs_module_text("ToScene"));
	label->setStyleSheet("font-weight: bold;");
	mainLayout->addWidget(label, 0, idx++, Qt::AlignCenter);
	label = new QLabel(obs_module_text("Transition"));
	label->setStyleSheet("font-weight: bold;");
	mainLayout->addWidget(label, 0, idx++, Qt::AlignCenter);
	label = new QLabel(obs_module_text("Duration"));
	label->setStyleSheet("font-weight: bold;");
	mainLayout->addWidget(label, 0, idx++, Qt::AlignCenter);
	QCheckBox *checkbox = new QCheckBox;
	mainLayout->addWidget(checkbox, 0, idx++, Qt::AlignCenter);

	idx = 0;
	fromCombo = new QComboBox();
	fromCombo->setEditable(true);
	fromCombo->addItem("", QByteArray(""));
	fromCombo->addItem(obs_module_text("Any"), QByteArray("Any"));
	mainLayout->addWidget(fromCombo, 1, idx++);
	toCombo = new QComboBox();
	toCombo->setEditable(true);
	toCombo->addItem("", QByteArray(""));
	toCombo->addItem(obs_module_text("Any"), QByteArray("Any"));
	mainLayout->addWidget(toCombo, 1, idx++);

	for (size_t i = 0; i < scenes.sources.num; i++) {
		string sceneName = obs_source_get_name(scenes.sources.array[i]);
		fromCombo->addItem(QString::fromUtf8(sceneName.c_str()),
				   QByteArray(sceneName.c_str()));
		toCombo->addItem(QString::fromUtf8(sceneName.c_str()),
				 QByteArray(sceneName.c_str()));
	}

	connect(fromCombo, SIGNAL(editTextChanged(const QString &)),
		SLOT(RefreshTable()));
	connect(toCombo, SIGNAL(editTextChanged(const QString &)),
		SLOT(RefreshTable()));

	transitionCombo = new QComboBox();
	for (size_t i = 0; i < transitions.sources.num; i++) {
		string trName =
			obs_source_get_name(transitions.sources.array[i]);
		transitionCombo->addItem(QString::fromUtf8(trName.c_str()),
					 QByteArray(trName.c_str()));
	}
	mainLayout->addWidget(transitionCombo, 1, idx++);
	auto durationArea = new QWidget;
	auto durationLayout = new QHBoxLayout;
	durationArea->setLayout(durationLayout);
	durationCheckbox = new QCheckBox;
	durationLayout->addWidget(checkbox);
	durationSpin = new QSpinBox;
	durationSpin->setMinimum(50);
	durationSpin->setMaximum(20000);
	durationSpin->setSingleStep(50);
	durationSpin->setValue(500);
	durationSpin->setSuffix("ms");
	durationLayout->addWidget(durationSpin);
	mainLayout->addWidget(durationArea, 1, idx++);
	QPushButton *addButton = new QPushButton(obs_module_text("Set"));
	connect(addButton, &QPushButton::clicked, [this]() { AddClicked(); });
	mainLayout->addWidget(addButton, 1, idx++, Qt::AlignCenter);

	RefreshTable();

	QWidget *controlArea = new QWidget;
	controlArea->setLayout(mainLayout);
	controlArea->setSizePolicy(QSizePolicy::Preferred,
				   QSizePolicy::Preferred);

	QVBoxLayout *vlayout = new QVBoxLayout;
	vlayout->addWidget(controlArea);
	//vlayout->setAlignment(controlArea, Qt::AlignTop);
	QWidget *widget = new QWidget;
	widget->setLayout(vlayout);
	widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

	QScrollArea *scrollArea = new QScrollArea;
	scrollArea->setWidget(widget);
	scrollArea->setWidgetResizable(true);

	QPushButton *closeButton = new QPushButton(obs_module_text("Close"));
	QPushButton *deleteButton = new QPushButton(obs_module_text("Delete"));

	QHBoxLayout *bottomLayout = new QHBoxLayout;
	bottomLayout->addWidget(deleteButton, 0, Qt::AlignLeft);
	bottomLayout->addWidget(closeButton, 0, Qt::AlignRight);

	connect(deleteButton, &QPushButton::clicked,
		[this]() { DeleteClicked(); });
	connect(closeButton, &QPushButton::clicked, [this]() { close(); });

	vlayout = new QVBoxLayout;
	vlayout->setContentsMargins(11, 11, 11, 11);
	vlayout->addWidget(scrollArea);
	vlayout->addLayout(bottomLayout);
	setLayout(vlayout);

	setWindowTitle(obs_module_text("TransitionTable"));
	setSizeGripEnabled(true);

	setMinimumSize(500, 300);
}

TransitionTableDialog::~TransitionTableDialog()
{
	obs_frontend_source_list_free(&scenes);
	obs_frontend_source_list_free(&transitions);
}

void TransitionTableDialog::AddClicked()
{
	const auto fromScene = fromCombo->currentText();
	const auto toScene = toCombo->currentText();
	const auto transition = transitionCombo->currentText();
	if (fromScene.isEmpty() || toScene.isEmpty() || transition.isEmpty())
		return;
	auto &t = transition_table[fromScene.toUtf8().constData()]
				  [toScene.toUtf8().constData()];
	t.transition = transition.toUtf8().constData();
	t.duration = durationSpin->value();
	RefreshTable();
}

void TransitionTableDialog::DeleteClicked()
{
	for (auto row = 2; row < mainLayout->rowCount(); row++) {
		auto *item = mainLayout->itemAtPosition(row, 4);
		if (!item)
			continue;
		auto *checkBox = dynamic_cast<QCheckBox *>(item->widget());
		if (!checkBox || !checkBox->isChecked())
			continue;

		item = mainLayout->itemAtPosition(row, 0);
		auto *label = dynamic_cast<QLabel *>(item->widget());
		if (!label)
			continue;
		string fromScene = label->text().toUtf8().constData();
		auto fs_it = transition_table.find(fromScene);
		if (fs_it == transition_table.end())
			continue;
		item = mainLayout->itemAtPosition(row, 1);
		label = dynamic_cast<QLabel *>(item->widget());
		if (!label)
			continue;
		string toScene = label->text().toUtf8().constData();
		auto ts_it = fs_it->second.find(toScene);
		if (ts_it == fs_it->second.end())
			continue;
		fs_it->second.erase(ts_it);
	}
	RefreshTable();
}

void TransitionTableDialog::RefreshTable()
{
	const auto fromScene = fromCombo->currentText();
	const auto toScene = toCombo->currentText();
	for (auto row = mainLayout->rowCount() - 1; row >= 2; row--) {
		for (auto col = mainLayout->columnCount() - 1; col >= 0;
		     col--) {
			auto *item = mainLayout->itemAtPosition(row, col);
			if (item) {
				mainLayout->removeItem(item);
				delete item->widget();
				delete item;
			}
		}
	}
	auto row = 2;
	for (const auto &it : transition_table) {
		for (const auto &it2 : it.second) {
			if (!fromScene.isEmpty() &&
			    fromScene != QString::fromUtf8(it.first.c_str()))
				continue;
			if (!toScene.isEmpty() &&
			    toScene != QString::fromUtf8(it2.first.c_str()))
				continue;
			auto col = 0;
			auto *label =
				new QLabel(QString::fromUtf8(it.first.c_str()));
			if (it.first == "Any") {
				label->setProperty("themeID", "good");
			} else {
				auto scene = obs_get_source_by_name(
					it.first.c_str());
				if (scene) {
					obs_source_release(scene);
				} else {
					label->setProperty("themeID", "error");
				}
			}
			mainLayout->addWidget(label, row, col++);
			label = new QLabel(
				QString::fromUtf8(it2.first.c_str()));
			if (it2.first == "Any") {
				label->setProperty("themeID", "good");
			} else {
				auto scene = obs_get_source_by_name(
					it2.first.c_str());
				if (scene) {
					obs_source_release(scene);
				} else {
					label->setProperty("themeID", "error");
				}
			}
			mainLayout->addWidget(label, row, col++);
			label = new QLabel(QString::fromUtf8(
				it2.second.transition.c_str()));
			mainLayout->addWidget(label, row, col++);
			label = new QLabel(QString::fromUtf8(
				(to_string(it2.second.duration) + "ms")
					.c_str()));
			mainLayout->addWidget(label, row, col++,
					      Qt::AlignRight);
			auto *checkBox = new QCheckBox;
			mainLayout->addWidget(checkBox, row, col++,
					      Qt::AlignCenter);
			row++;
		}
	}
}
