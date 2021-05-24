
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
#include <QMouseEvent>
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

int transition_table_width = 0;
int transition_table_height = 0;

obs_hotkey_pair_id transition_table_hotkey = OBS_INVALID_HOTKEY_PAIR_ID;

static void load_transition_matrix(obs_data_t *obj)
{
	obs_data_array_t *transitions = obs_data_get_array(obj, "matrix");
	if (!transitions)
		return;
	const size_t count = obs_data_array_count(transitions);
	for (size_t i = 0; i < count; i++) {
		obs_data_t *transition = obs_data_array_item(transitions, i);
		string fromScene = obs_data_get_string(transition, "scene");
		obs_data_array_t *data = obs_data_get_array(transition, "data");
		const size_t transition_count = obs_data_array_count(data);
		for (size_t j = 0; j < transition_count; j++) {
			obs_data_t *transition2 = obs_data_array_item(data, j);
			string toScene = obs_data_get_string(transition2, "to");
			if (!fromScene.empty() && !toScene.empty()) {
				transition_table[fromScene][toScene].transition =
					obs_data_get_string(transition2,
							    "transition");
				transition_table[fromScene][toScene].duration =
					obs_data_get_int(transition2,
							 "duration");
			}
			obs_data_release(transition2);
		}
		obs_data_array_release(data);
		obs_data_release(transition);
	}
	obs_data_array_release(transitions);
}

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
		if (transition_table_width > 500 &&
		    transition_table_height > 300) {
			obs_data_set_int(obj, "dialog_width",
					 transition_table_width);
			obs_data_set_int(obj, "dialog_height",
					 transition_table_height);
		}
		obs_data_array_t *data0 = nullptr;
		obs_data_array_t *data1 = nullptr;
		obs_hotkey_pair_save(transition_table_hotkey, &data0, &data1);
		if (data0) {
			obs_data_set_array(obj, "enable_hotkey", data0);
			obs_data_array_release(data0);
		}
		if (data1) {
			obs_data_set_array(obj, "disable_hotkey", data1);
			obs_data_array_release(data1);
		}
		obs_data_set_obj(save_data, "transition-table", obj);
		obs_data_array_release(transitions);
		obs_data_release(obj);
	} else {
		transition_table.clear();
		obs_data_t *obj =
			obs_data_get_obj(save_data, "transition-table");
		if (obj) {
			transition_table_width =
				obs_data_get_int(obj, "dialog_width");
			transition_table_height =
				obs_data_get_int(obj, "dialog_height");

			obs_hotkey_pair_load(
				transition_table_hotkey,
				obs_data_get_array(obj, "enable_hotkey"),
				obs_data_get_array(obj, "disable_hotkey"));
			obs_data_array_t *transitions =
				obs_data_get_array(obj, "transitions");
			if (transitions) {
				size_t count =
					obs_data_array_count(transitions);
				for (size_t i = 0; i < count; i++) {
					obs_data_t *transition =
						obs_data_array_item(transitions,
								    i);
					string fromScene = obs_data_get_string(
						transition, "from_scene");
					string toScene = obs_data_get_string(
						transition, "to_scene");
					string transitionName =
						obs_data_get_string(
							transition,
							"transition");
					bool custom_duration = obs_data_get_bool(
						transition, "custom_duration");
					const uint32_t duration =
						obs_data_get_int(transition,
								 "duration");
					transition_table[fromScene][toScene]
						.transition = transitionName;
					transition_table[fromScene][toScene]
						.duration = duration;
				}
				obs_data_array_release(transitions);
			}
			obs_data_release(obj);
		} else {
			obj = obs_data_get_obj(save_data,
					       "obs-transition-matrix");
			if (obj) {
				load_transition_matrix(obj);
				obs_data_release(obj);
			}

			obs_frontend_source_list scenes = {};
			obs_frontend_get_scenes(&scenes);
			for (size_t i = 0; i < scenes.sources.num; i++) {
				obs_data_t *data =
					obs_source_get_private_settings(
						scenes.sources.array[i]);
				string transitionName =
					obs_data_get_string(data, "transition");
				string sceneName = obs_source_get_name(
					scenes.sources.array[i]);
				if (!transitionName.empty()) {
					transition_table["Any"][sceneName]
						.transition = transitionName;
					transition_table["Any"][sceneName]
						.duration = obs_data_get_int(
						data, "transition_duration");
				}
				obs_data_release(data);
			}
			obs_frontend_source_list_free(&scenes);
		}
	}
}

static void clear_transition_overrides()
{
	obs_frontend_source_list scenes = {};
	obs_frontend_get_scenes(&scenes);
	for (size_t i = 0; i < scenes.sources.num; i++) {
		obs_data_t *data = obs_source_get_private_settings(
			scenes.sources.array[i]);
		obs_data_erase(data, "transition");
		obs_data_release(data);
	}
	obs_frontend_source_list_free(&scenes);
}

static void set_transition_overrides()
{
	obs_source_t *scene = obs_frontend_get_current_scene();
	string fromScene = obs_source_get_name(scene);
	obs_source_release(scene);

	auto fs_it = fromScene.empty() ? transition_table.end()
				       : transition_table.find(fromScene);
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

static bool transition_table_enabled = true;

static void frontend_event(enum obs_frontend_event event, void *)
{
	if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED) {
		if (transition_table_enabled)
			set_transition_overrides();
	} else if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP) {
		transition_table.clear();
	}
}

static void source_rename(void *data, calldata_t *call_data)
{
	string new_name = calldata_string(call_data, "new_name");
	string prev_name = calldata_string(call_data, "prev_name");
	auto it = transition_table.find(prev_name);
	if (it != transition_table.end()) {
		transition_table[new_name] = it->second;
		transition_table.erase(it);
	}
	for (const auto &it : transition_table) {
		auto it2 = it.second.find(prev_name);
		if (it2 != it.second.end()) {
			transition_table[it.first][new_name] = it2->second;
			transition_table[it.first].erase(it2);
		}
	}
}

bool enable_hotkey(void *data, obs_hotkey_pair_id id, obs_hotkey_t *hotkey,
		   bool pressed)
{
	if (!transition_table_enabled && pressed) {
		transition_table_enabled = true;
		set_transition_overrides();
		return true;
	}
	return false;
}

bool disable_hotkey(void *data, obs_hotkey_pair_id id, obs_hotkey_t *hotkey,
		    bool pressed)
{
	if (transition_table_enabled && pressed) {
		transition_table_enabled = false;
		clear_transition_overrides();
		return true;
	}
	return false;
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
	signal_handler_connect(obs_get_signal_handler(), "source_rename",
			       source_rename, nullptr);

	transition_table_hotkey = obs_hotkey_pair_register_frontend(
		"transition-table.enable",
		obs_module_text("TransitionTable.Enable"),
		"transition-table.disable",
		obs_module_text("TransitionTable.Disable"), enable_hotkey,
		disable_hotkey, nullptr, nullptr);

	return true;
}

void obs_module_unload(void)
{
	obs_hotkey_pair_unregister(transition_table_hotkey);
	obs_frontend_remove_save_callback(frontend_save_load, nullptr);
	obs_frontend_remove_event_callback(frontend_event, nullptr);
	signal_handler_disconnect(obs_get_signal_handler(), "source_rename",
				  source_rename, nullptr);
}

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
	mainLayout->setColumnStretch(0, 1);
	mainLayout->setColumnStretch(1, 1);
	mainLayout->setColumnStretch(2, 1);

	connect(checkbox, &QCheckBox::stateChanged,
		[this]() { SelectAllChanged(); });

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
	durationSpin = new QSpinBox;
	durationSpin->setMinimum(50);
	durationSpin->setMaximum(20000);
	durationSpin->setSingleStep(50);
	durationSpin->setValue(500);
	durationSpin->setSuffix("ms");
	mainLayout->addWidget(durationSpin, 1, idx++);
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
	if (transition_table_width > 500 && transition_table_height > 300) {
		resize(transition_table_width, transition_table_height);
	}
}

TransitionTableDialog::~TransitionTableDialog()
{
	auto size = this->size();
	transition_table_width = size.width();
	transition_table_height = size.height();
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
	if (transition_table_enabled)
		set_transition_overrides();
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
	if (transition_table_enabled)
		set_transition_overrides();
}

void TransitionTableDialog::SelectAllChanged()
{
	auto *item = mainLayout->itemAtPosition(0, 4);
	auto *checkBox = dynamic_cast<QCheckBox *>(item->widget());
	bool checked = checkBox && checkBox->isChecked();
	for (auto row = 2; row < mainLayout->rowCount(); row++) {
		item = mainLayout->itemAtPosition(row, 4);
		if (!item)
			continue;
		auto *checkBox = dynamic_cast<QCheckBox *>(item->widget());
		if (!checkBox)
			continue;
		checkBox->setChecked(checked);
	}
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
		if (!fromScene.isEmpty() &&
		    fromScene != QString::fromUtf8(it.first.c_str()))
			continue;
		for (const auto &it2 : it.second) {
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

void TransitionTableDialog::mouseDoubleClickEvent(QMouseEvent *event)
{
	QWidget *widget = childAt(event->pos());
	if (!widget)
		return;
	int index = mainLayout->indexOf(widget);
	if (index < 0)
		return;

	int row, column, row_span, col_span;
	mainLayout->getItemPosition(index, &row, &column, &row_span, &col_span);
	if (row < 2)
		return;

	const QString from =
		dynamic_cast<QLabel *>(
			mainLayout->itemAtPosition(row, 0)->widget())
			->text();
	fromCombo->setCurrentText(from);
	const QString to = dynamic_cast<QLabel *>(
				   mainLayout->itemAtPosition(row, 1)->widget())
				   ->text();
	toCombo->setCurrentText(to);
}
