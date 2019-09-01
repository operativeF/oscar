/* SleepLib AppSettings Header
 *
 * This file for all settings related stuff to clean up Preferences & Profiles.
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the sourcecode. */

#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <QDateTime>
#include "preferences.h"
#include "common.h"
#include "version.h"


class Preferences;

enum OverviewLinechartModes { OLC_Bartop, OLC_Lines };


// ApplicationWideSettings Strings
const QString STR_CS_UserEventPieChart = "UserEventPieChart";
const QString STR_IS_Multithreading = "EnableMultithreading";
const QString STR_AS_GraphHeight = "GraphHeight";
const QString STR_AS_DailyPanelWidth = "DailyPanelWidth";
const QString STR_AS_RightPanelWidth = "RightPanelWidth";
const QString STR_AS_AntiAliasing = "UseAntiAliasing";
const QString STR_AS_GraphSnapshots = "EnableGraphSnapshots";  // Obsolete, replaced by ShowPieChart
const QString STR_AS_ShowPieChart = "EnablePieChart";
const QString STR_AS_Animations = "AnimationsAndTransitions";
const QString STR_AS_SquareWave = "SquareWavePlots";
const QString STR_AS_OverlayType = "OverlayType";
const QString STR_AS_OverviewLinechartMode = "OverviewLinechartMode";
const QString STR_AS_UsePixmapCaching = "UsePixmapCaching";
const QString STR_AS_AllowYAxisScaling = "AllowYAxisScaling";
const QString STR_AS_IncludeSerial = "IncludeSerial";
const QString STR_AS_GraphTooltips = "GraphTooltips";
const QString STR_AS_LineThickness = "LineThickness";
const QString STR_AS_LineCursorMode = "LineCursorMode";
const QString STR_AS_CalendarVisible = "CalendarVisible";
const QString STR_AS_RightSidebarVisible = "RightSidebarVisible";
const QString STR_US_TooltipTimeout = "TooltipTimeout";
const QString STR_US_ScrollDampening = "ScrollDampening";
const QString STR_US_ShowDebug = "ShowDebug";
const QString STR_US_ShowPerformance = "ShowPerformance";
const QString STR_US_ShowSerialNumbers = "ShowSerialNumbers";
const QString STR_US_OpenTabAtStart = "OpenTabAtStart";
const QString STR_US_OpenTabAfterImport = "OpenTabAfterImport";
const QString STR_US_AutoLaunchImport = "AutoLaunchImport";
const QString STR_US_RemoveCardReminder = "RemoveCardReminder";
const QString STR_IS_CacheSessions = "MemoryHog";

const QString STR_GEN_AutoOpenLastUsed = "AutoOpenLastUsed";
const QString STR_GEN_Language = "Language";
const QString STR_PREF_VersionString = "VersionString";
const QString STR_GEN_ShowAboutDialog = "ShowAboutDialog";
#ifndef NO_UPDATER
const QString STR_GEN_UpdatesLastChecked = "UpdatesLastChecked";
const QString STR_GEN_UpdatesAutoCheck = "Updates_AutoCheck";
const QString STR_GEN_UpdateCheckFrequency = "Updates_CheckFrequency";
const QString STR_PREF_AllowEarlyUpdates = "AllowEarlyUpdates";
#endif


class AppWideSetting: public PrefSettings
{
public:
  AppWideSetting(Preferences *pref)
    : PrefSettings(pref)
  {
//    m_multithreading = initPref(STR_IS_Multithreading, idealThreads() > 1).toBool();
      m_multithreading = false;     // too dangerous to allow 
      m_showPerformance = initPref(STR_US_ShowPerformance, false).toBool();
      m_showDebug = initPref(STR_US_ShowDebug, false).toBool();
      initPref(STR_AS_CalendarVisible, false);
      m_scrollDampening = initPref(STR_US_ScrollDampening, (int)50).toInt();
      m_tooltipTimeout = initPref(STR_US_TooltipTimeout, (int)2500).toInt();
      m_graphHeight=initPref(STR_AS_GraphHeight, 180).toInt();
      initPref(STR_AS_DailyPanelWidth, 250.0);
      initPref(STR_AS_RightPanelWidth, 230.0);
      m_antiAliasing=initPref(STR_AS_AntiAliasing, true).toBool();
//      initPref(STR_AS_GraphSnapshots, true);
      initPref(STR_AS_IncludeSerial, false);
      initPref(STR_AS_ShowPieChart, false);
      m_animations = initPref(STR_AS_Animations, true).toBool();
      m_squareWavePlots = initPref(STR_AS_SquareWave, false).toBool();
      initPref(STR_AS_AllowYAxisScaling, true);
      m_graphTooltips = initPref(STR_AS_GraphTooltips, true).toBool();
      m_usePixmapCaching = initPref(STR_AS_UsePixmapCaching, false).toBool();
      m_odt = (OverlayDisplayType)initPref(STR_AS_OverlayType, (int)ODT_Bars).toInt();
      m_olm = (OverviewLinechartModes)initPref(STR_AS_OverviewLinechartMode, (int)OLC_Bartop).toInt();
      m_lineThickness=initPref(STR_AS_LineThickness, 1.0).toFloat();
      m_lineCursorMode = initPref(STR_AS_LineCursorMode, true).toBool();
      initPref(STR_AS_RightSidebarVisible, false);
      initPref(STR_CS_UserEventPieChart, false);
      initPref(STR_US_ShowSerialNumbers, false);
      initPref(STR_US_OpenTabAtStart, 1);
      initPref(STR_US_OpenTabAfterImport, 0);
      initPref(STR_US_AutoLaunchImport, false);
      m_cacheSessions = initPref(STR_IS_CacheSessions, false).toBool();
      initPref(STR_US_RemoveCardReminder, true);
      m_profileName = initPref(STR_GEN_Profile, "").toString();
      initPref(STR_GEN_AutoOpenLastUsed, true);

#ifndef NO_UPDATER
      initPref(STR_GEN_UpdatesAutoCheck, true);
      initPref(STR_GEN_UpdateCheckFrequency, 7);
      initPref(STR_PREF_AllowEarlyUpdates, false);
      initPref(STR_GEN_UpdatesLastChecked, QDateTime());
#endif
      initPref(STR_PREF_VersionString, VersionString);
      m_language = initPref(STR_GEN_Language, "en_US").toString();
      initPref(STR_GEN_ShowAboutDialog, 0);  // default to about screen, set to -1 afterwards
  }

  bool m_usePixmapCaching, m_antiAliasing, m_squareWavePlots,m_graphTooltips, m_lineCursorMode, m_animations;
  bool m_showPerformance, m_showDebug;
  int m_tooltipTimeout, m_graphHeight, m_scrollDampening;
  bool m_multithreading, m_cacheSessions;
  float m_lineThickness;

  OverlayDisplayType m_odt;
  OverviewLinechartModes m_olm;
  QString m_profileName, m_language;

  QString versionString() const { return getPref(STR_PREF_VersionString).toString(); }
#ifndef NO_UPDATER
  bool updatesAutoCheck() const { return getPref(STR_GEN_UpdatesAutoCheck).toBool(); }
  bool allowEarlyUpdates() const { return getPref(STR_PREF_AllowEarlyUpdates).toBool(); }
  QDateTime updatesLastChecked() const { return getPref(STR_GEN_UpdatesLastChecked).toDateTime(); }
  int updateCheckFrequency() const { return getPref(STR_GEN_UpdateCheckFrequency).toInt(); }
#endif
  int showAboutDialog() const { return getPref(STR_GEN_ShowAboutDialog).toInt(); }
  void setShowAboutDialog(int tab) {setPref(STR_GEN_ShowAboutDialog, tab); }

  inline const QString & profileName() const { return m_profileName; }
  bool autoLaunchImport() const { return getPref(STR_US_AutoLaunchImport).toBool(); }
  bool cacheSessions() const { return m_cacheSessions; }
  inline bool multithreading() const { return m_multithreading; }
  bool showDebug() const { return m_showDebug; }
  bool showPerformance() const { return m_showPerformance; }
  //! \brief Whether to show the calendar
  bool calendarVisible() const { return getPref(STR_AS_CalendarVisible).toBool(); }
  inline int scrollDampening() const { return m_scrollDampening; }
  inline int tooltipTimeout() const { return m_tooltipTimeout; }
  //! \brief Returns the normal (unscaled) height of a graph
  inline int graphHeight() const { return m_graphHeight; }
  //! \brief Returns the normal (unscaled) height of a graph
  int dailyPanelWidth() const { return getPref(STR_AS_DailyPanelWidth).toInt(); }
  //! \brief Returns the normal (unscaled) height of a graph
  int rightPanelWidth() const { return getPref(STR_AS_RightPanelWidth).toInt(); }
  //! \brief Returns true if AntiAliasing (the graphical smoothing method) is enabled
  inline bool antiAliasing() const { return m_antiAliasing; }
  //! \brief Returns true if renderPixmap function is in use, which takes snapshots of graphs
  bool showPieChart() const { return getPref(STR_AS_ShowPieChart).toBool(); }
  //! \brief Returns true if Graphical animations & Transitions will be drawn
  bool animations() const { return m_animations; }
  //! \brief Returns true if PixmapCaching acceleration will be used
  inline bool usePixmapCaching() const { return m_usePixmapCaching; }
  //! \brief Returns true if Square Wave plots are preferred (where possible)
  inline bool squareWavePlots() const { return m_squareWavePlots; }
  //! \brief Whether to allow double clicking on Y-Axis labels to change vertical scaling mode
  bool allowYAxisScaling() const { return getPref(STR_AS_AllowYAxisScaling).toBool(); }
  //! \brief Whether to include serial number in machine settings changes report
  bool includeSerial() const { return getPref(STR_AS_IncludeSerial).toBool(); }
  //! \brief Whether to show graph tooltips
  inline bool graphTooltips() const { return m_graphTooltips; }
  //! \brief Pen width of line plots
  inline float lineThickness() const { return m_lineThickness; }
  //! \brief Whether to show line cursor
  inline bool lineCursorMode() const { return m_lineCursorMode; }
  //! \brief Whether to show the right sidebar
  bool rightSidebarVisible() const { return getPref(STR_AS_RightSidebarVisible).toBool(); }
  //! \brief Returns the type of overlay flags (which are displayed over the Flow Waveform)
  inline OverlayDisplayType overlayType() const { return m_odt; }
  //! \brief Returns the display type of Overview pages linechart
  inline OverviewLinechartModes overviewLinechartMode() const { return m_olm; }
  bool userEventPieChart() const { return getPref(STR_CS_UserEventPieChart).toBool(); }
  bool showSerialNumbers() const { return getPref(STR_US_ShowSerialNumbers).toBool(); }
  int openTabAtStart() const { return getPref(STR_US_OpenTabAtStart).toInt(); }
  int openTabAfterImport() const { return getPref(STR_US_OpenTabAfterImport).toInt(); }
  bool removeCardReminder() const { return getPref(STR_US_RemoveCardReminder).toBool(); }
  bool autoOpenLastUsed() const { return getPref(STR_GEN_AutoOpenLastUsed).toBool(); }
  inline const QString & language() const { return m_language; }

  void setProfileName(QString name) { setPref(STR_GEN_Profile, m_profileName=name); }
  void setAutoLaunchImport(bool b) { setPref(STR_US_AutoLaunchImport, b); }
  void setCacheSessions(bool c) { setPref(STR_IS_CacheSessions, m_cacheSessions=c); }
// force multithreading to false until proven OK
  void setMultithreading(bool b) { Q_UNUSED(b) setPref(STR_IS_Multithreading, m_multithreading = false); }
  void setShowDebug(bool b) { setPref(STR_US_ShowDebug, m_showDebug=b); }
  void setShowPerformance(bool b) { setPref(STR_US_ShowPerformance, m_showPerformance=b); }
  //! \brief Sets whether to display the (Daily View) Calendar
  void setCalendarVisible(bool b) { setPref(STR_AS_CalendarVisible, b); }
  void setScrollDampening(int i) { setPref(STR_US_ScrollDampening, m_scrollDampening=i); }
  void setTooltipTimeout(int i) { setPref(STR_US_TooltipTimeout, m_tooltipTimeout=i); }
  //! \brief Set the normal (unscaled) height of a graph.
  void setGraphHeight(int height) { setPref(STR_AS_GraphHeight, m_graphHeight=height); }
  //! \brief Set the normal (unscaled) height of a graph.
  void setDailyPanelWidth(int width) { setPref(STR_AS_DailyPanelWidth, width); }
  //! \brief Set the normal (unscaled) height of a graph.
  void setRightPanelWidth(int width) { setPref(STR_AS_RightPanelWidth, width); }
  //! \brief Set to true to turn on AntiAliasing (the graphical smoothing method)
  void setAntiAliasing(bool aa) { setPref(STR_AS_AntiAliasing, m_antiAliasing=aa); }
  //! \brief Set to true if renderPixmap functions are in use, which takes snapshots of graphs.
  void setShowPieChart(bool gs) { setPref(STR_AS_ShowPieChart, gs); }
  //! \brief Set to true if Graphical animations & Transitions will be drawn
  void setAnimations(bool anim) { setPref(STR_AS_Animations, m_animations=anim); }
  //! \brief Set to true to use Pixmap Caching of Text and other graphics caching speedup techniques
  void setUsePixmapCaching(bool b) { setPref(STR_AS_UsePixmapCaching, m_usePixmapCaching=b); }
  //! \brief Set whether or not to useSquare Wave plots (where possible)
  void setSquareWavePlots(bool sw) { setPref(STR_AS_SquareWave, m_squareWavePlots=sw); }
  //! \brief Sets the type of overlay flags (which are displayed over the Flow Waveform)
  void setOverlayType(OverlayDisplayType odt) { setPref(STR_AS_OverlayType, (int)(m_odt=odt)); }
  //! \brief Sets whether to allow double clicking on Y-Axis labels to change vertical scaling mode
  void setAllowYAxisScaling(bool b) { setPref(STR_AS_AllowYAxisScaling, b); }
  //! \brief Sets whether to include machine serial number on machine settings report
  void setIncludeSerial(bool b) { setPref(STR_AS_IncludeSerial, b); }
  //! \brief Sets whether to allow double clicking on Y-Axis labels to change vertical scaling mode
  void setGraphTooltips(bool b) { setPref(STR_AS_GraphTooltips, m_graphTooltips=b); }
  //! \brief Sets the type of overlay flags (which are displayed over the Flow Waveform)
  void setOverviewLinechartMode(OverviewLinechartModes olm) { setPref(STR_AS_OverviewLinechartMode, (int)(m_olm=olm)); }
  //! \brief Set the pen width of line plots.
  void setLineThickness(float size) { setPref(STR_AS_LineThickness, m_lineThickness=size); }
  //! \brief Sets whether to display Line Cursor
  void setLineCursorMode(bool b) { setPref(STR_AS_LineCursorMode, m_lineCursorMode=b); }
  //! \brief Sets whether to display the right sidebar
  void setRightSidebarVisible(bool b) { setPref(STR_AS_RightSidebarVisible, b); }
  void setUserEventPieChart(bool b) { setPref(STR_CS_UserEventPieChart, b); }
  void setShowSerialNumbers(bool enabled) { setPref(STR_US_ShowSerialNumbers, enabled); }
  void setOpenTabAtStart(int idx) { setPref(STR_US_OpenTabAtStart, idx); }
  void setOpenTabAfterImport(int idx) { setPref(STR_US_OpenTabAfterImport, idx); }
  void setRemoveCardReminder(bool b) { setPref(STR_US_RemoveCardReminder, b); }

  void setVersionString(QString version) { setPref(STR_PREF_VersionString, version); }
#ifndef NO_UPDATER
  void setUpdatesAutoCheck(bool b) { setPref(STR_GEN_UpdatesAutoCheck, b); }
  void setAllowEarlyUpdates(bool b)  { setPref(STR_PREF_AllowEarlyUpdates, b); }
  void setUpdatesLastChecked(QDateTime datetime) { setPref(STR_GEN_UpdatesLastChecked, datetime); }
  void setUpdateCheckFrequency(int freq) { setPref(STR_GEN_UpdateCheckFrequency,freq); }
#endif
  void setAutoOpenLastUsed(bool b) { setPref(STR_GEN_AutoOpenLastUsed , b); }
  void setLanguage(QString language) { setPref(STR_GEN_Language, m_language=language); }

};


extern AppWideSetting *AppSetting;



#endif // APPSETTINGS_H
