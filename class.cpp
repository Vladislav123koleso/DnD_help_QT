#include "class.h"

#include <QJsonArray>

namespace {

QJsonArray stringListToJson(const QStringList &values)
{
	QJsonArray array;
	for (const QString &value : values) {
		const QString trimmed = value.trimmed();
		if (!trimmed.isEmpty()) {
			array.append(trimmed);
		}
	}
	return array;
}

QStringList jsonToStringList(const QJsonValue &value)
{
	QStringList result;
	const QJsonArray array = value.toArray();
	for (const QJsonValue &entry : array) {
		const QString text = entry.toString().trimmed();
		if (!text.isEmpty()) {
			result << text;
		}
	}
	result.removeDuplicates();
	return result;
}

QJsonArray sectionListToJson(const QList<ClassSection> &sections)
{
	QJsonArray array;
	for (const ClassSection &section : sections) {
		if (!section.title.trimmed().isEmpty()) {
			array.append(section.toJson());
		}
	}
	return array;
}

QList<ClassSection> jsonToSectionList(const QJsonValue &value)
{
	QList<ClassSection> sections;
	const QJsonArray array = value.toArray();
	for (const QJsonValue &entry : array) {
		if (entry.isObject()) {
			sections.append(ClassSection::fromJson(entry.toObject()));
		}
	}
	return sections;
}

QJsonArray progressionListToJson(const QList<ClassLevelProgression> &progression)
{
	QJsonArray array;
	for (const ClassLevelProgression &entry : progression) {
		if (entry.level > 0) {
			array.append(entry.toJson());
		}
	}
	return array;
}

QList<ClassLevelProgression> jsonToProgressionList(const QJsonValue &value)
{
	QList<ClassLevelProgression> progression;
	const QJsonArray array = value.toArray();
	for (const QJsonValue &entry : array) {
		if (entry.isObject()) {
			progression.append(ClassLevelProgression::fromJson(entry.toObject()));
		}
	}
	return progression;
}

QJsonArray subclassListToJson(const QList<ClassSubclass> &subclasses)
{
	QJsonArray array;
	for (const ClassSubclass &subclass : subclasses) {
		if (!subclass.name.trimmed().isEmpty()) {
			array.append(subclass.toJson());
		}
	}
	return array;
}

QList<ClassSubclass> jsonToSubclassList(const QJsonValue &value)
{
	QList<ClassSubclass> subclasses;
	const QJsonArray array = value.toArray();
	for (const QJsonValue &entry : array) {
		if (entry.isObject()) {
			subclasses.append(ClassSubclass::fromJson(entry.toObject()));
		}
	}
	return subclasses;
}

}

QJsonObject ClassSection::toJson() const
{
	QJsonObject object;
	object.insert("title", title.trimmed());
	object.insert("description", description.trimmed());
	object.insert("levelText", levelText.trimmed());
	object.insert("levelRequirement", levelRequirement);
	object.insert("optional", optional);
	return object;
}

ClassSection ClassSection::fromJson(const QJsonObject &object)
{
	ClassSection section;
	section.title = object.value("title").toString().trimmed();
	section.description = object.value("description").toString().trimmed();
	section.levelText = object.value("levelText").toString().trimmed();
	section.levelRequirement = object.value("levelRequirement").toInt();
	section.optional = object.value("optional").toBool(false);
	return section;
}

QJsonObject ClassLevelProgression::toJson() const
{
	QJsonObject object;
	object.insert("level", level);
	object.insert("proficiencyBonus", proficiencyBonus);
	object.insert("features", stringListToJson(features));
	return object;
}

ClassLevelProgression ClassLevelProgression::fromJson(const QJsonObject &object)
{
	ClassLevelProgression entry;
	entry.level = object.value("level").toInt();
	entry.proficiencyBonus = object.value("proficiencyBonus").toInt();
	entry.features = jsonToStringList(object.value("features"));
	return entry;
}

QJsonObject ClassSubclass::toJson() const
{
	QJsonObject object;
	object.insert("name", name.trimmed());
	object.insert("description", description.trimmed());
	object.insert("sections", sectionListToJson(sections));
	return object;
}

ClassSubclass ClassSubclass::fromJson(const QJsonObject &object)
{
	ClassSubclass subclass;
	subclass.name = object.value("name").toString().trimmed();
	subclass.description = object.value("description").toString().trimmed();
	subclass.sections = jsonToSectionList(object.value("sections"));
	return subclass;
}

Class::Class() : slug(""), name(""), source(""), description("") {}

Class Class::fromJson(const QJsonObject &object)
{
	Class cls;
	cls.slug = object.value("slug").toString().trimmed();
	cls.name = object.value("name").toString().trimmed();
	cls.source = object.value("source").toString().trimmed();
	cls.description = object.value("description").toString().trimmed();
	cls.hitDie = object.value("hitDie").toInt(0);
	cls.primaryAbility = object.value("primaryAbility").toString().trimmed();
	cls.savingThrowProficiencies = jsonToStringList(object.value("savingThrowProficiencies"));
	cls.armorProficiencies = jsonToStringList(object.value("armorProficiencies"));
	cls.weaponProficiencies = jsonToStringList(object.value("weaponProficiencies"));
	cls.loadExtraData(object);
	return cls;
}

QJsonObject Class::toJson() const
{
	QJsonObject object;
	object.insert("slug", slug.trimmed());
	object.insert("name", name.trimmed());
	object.insert("source", source.trimmed());
	object.insert("description", description.trimmed());
	object.insert("hitDie", hitDie);
	object.insert("primaryAbility", primaryAbility.trimmed());
	object.insert("savingThrowProficiencies", stringListToJson(savingThrowProficiencies));
	object.insert("armorProficiencies", stringListToJson(armorProficiencies));
	object.insert("weaponProficiencies", stringListToJson(weaponProficiencies));

	const QJsonObject extra = extraDataToJson();
	for (auto it = extra.begin(); it != extra.end(); ++it) {
		object.insert(it.key(), it.value());
	}

	return object;
}

QJsonObject Class::extraDataToJson() const
{
	QJsonObject object;
	object.insert("toolProficiencies", stringListToJson(toolProficiencies));
	object.insert("skillChoices", stringListToJson(skillChoices));
	object.insert("skillChoiceCount", skillChoiceCount);
	object.insert("startingEquipment", stringListToJson(startingEquipment));
	object.insert("multiclassRequirement", multiclassRequirement.trimmed());
	object.insert("multiclassProficiencies", stringListToJson(multiclassProficiencies));
	object.insert("multiclassSpellcastingNote", multiclassSpellcastingNote.trimmed());
	object.insert("progression", progressionListToJson(progression));
	object.insert("featureSections", sectionListToJson(featureSections));
	object.insert("subclasses", subclassListToJson(subclasses));
	return object;
}

void Class::loadExtraData(const QJsonObject &object)
{
	toolProficiencies = jsonToStringList(object.value("toolProficiencies"));
	skillChoices = jsonToStringList(object.value("skillChoices"));
	skillChoiceCount = object.value("skillChoiceCount").toInt();
	startingEquipment = jsonToStringList(object.value("startingEquipment"));
	multiclassRequirement = object.value("multiclassRequirement").toString().trimmed();
	multiclassProficiencies = jsonToStringList(object.value("multiclassProficiencies"));
	multiclassSpellcastingNote = object.value("multiclassSpellcastingNote").toString().trimmed();
	progression = jsonToProgressionList(object.value("progression"));
	featureSections = jsonToSectionList(object.value("featureSections"));
	subclasses = jsonToSubclassList(object.value("subclasses"));
}
