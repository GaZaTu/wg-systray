#include <QApplication>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QProcess>
#include <QSettings>
#include <memory>
#include <string>
#include <set>

auto runWGQuick(std::string_view command, std::string_view intf) {
  QProcess process;
  process.setProcessChannelMode(QProcess::ForwardedChannels);
  process.start("pkexec", {"wg-quick", command.data(), intf.data()});

  return process.waitForFinished();
}

auto runWGShowInterfaces() {
  QProcess process;
  process.start("wg", {"show", "interfaces"});
  process.waitForFinished();

  QString result{process.readAllStandardOutput()};
  return result.trimmed().toStdString();
}

class App {
public:
  App() {
    auto read_etc_wireguard_action = systray_menu.addAction("ls /etc/wireguard");
    QObject::connect(read_etc_wireguard_action, &QAction::triggered, [this]() {
      QProcess process;
      process.start("pkexec", {"ls", "/etc/wireguard"});
      process.waitForFinished();

      QString stdout{process.readAllStandardOutput()};
      QStringList interfaces = stdout
        .split(QRegExp{"\\s+"})
        .filter(".conf")
        .replaceInStrings(".conf", "");

      QSettings settings;
      settings.setValue("interfaces", interfaces);
      settings.sync();

      populateSystrayMenu();
    });

    systray_menu.addSeparator();

    populateSystrayMenu();
    QObject::connect(&systray, &QSystemTrayIcon::activated, [this](auto reason) {
      populateSystrayMenu();
    });

    systray.setContextMenu(&systray_menu);
    systray.setVisible(true);
  }

private:
  QIcon default_icon{":/res/wg-icon-32x32.png"};
  QIcon check_icon{":/res/wg-check-icon-32x32.png"};

  QSystemTrayIcon systray{default_icon};
  QMenu systray_menu{"wg-systray"};

  bool intfExists(std::string_view intf) {
    for (auto other : systray_menu.actions()) {
      auto other_intf = other->text().toStdString();

      if (other_intf == intf) {
        return true;
      }
    }

    return false;
  }

  std::set<std::string> getWireguardInterfaces() {
    std::set<std::string> result;

    QSettings settings;

    auto interfaces = settings.value("interfaces").toStringList();

    for (const auto& intf : interfaces) {
      result.emplace(intf.toStdString());
    }

    return result;
  }

  void populateSystrayMenu() {
    auto active_intf = runWGShowInterfaces();
    auto interfaces = getWireguardInterfaces();

    for (auto action : systray_menu.actions()) {
      if (action->property("intf").isNull()) {
        continue;
      }

      auto intf = action->text().toStdString();

      if (interfaces.count(intf) == 0) {
        delete action;
      }
    }

    for (const auto& intf : interfaces) {
      if (intfExists(intf)) {
        continue;
      }

      auto action = systray_menu.addAction(intf.data());
      action->setProperty("intf", true);
      action->setCheckable(true);

      if (intf == active_intf) {
        action->setChecked(true);
        systray.setIcon(check_icon);
      }

      QObject::connect(action, &QAction::triggered, [this, action](auto checked) {
        auto intf = action->text().toStdString();

        if (checked) {
          for (auto other : systray_menu.actions()) {
            if (other->property("intf").isNull()) {
              continue;
            }

            auto other_intf = other->text().toStdString();

            if (other == action || !other->isChecked()) {
              continue;
            }

            if (runWGQuick("down", other_intf)) {
              other->setChecked(false);
              systray.setIcon(default_icon);
            }
          }

          if (runWGQuick("up", intf)) {
            systray.setIcon(check_icon);
          }
        } else {
          if (runWGQuick("down", intf)) {
            systray.setIcon(default_icon);
          }
        }
      });
    }
  }
};

int main(int argc, char** argv) {
  QCoreApplication::setOrganizationName(PROJECT_ORGANIZATION);
  QCoreApplication::setOrganizationDomain(PROJECT_ORGANIZATION);
  QCoreApplication::setApplicationName(PROJECT_NAME);
  QCoreApplication::setApplicationVersion(PROJECT_VERSION);

  QApplication qapp{argc, argv};

  App app;

  return qapp.exec();
}
