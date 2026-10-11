#include "hotkeys/NiriHotkeyBackend.h"

#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class NiriHotkeyBackendTest final : public QObject {
    Q_OBJECT

private slots:
    void detectsNiriWithoutRequiringDesktopName()
    {
        QVERIFY(NiriHotkeyBackend::isNiriSession(QStringLiteral("niri"), {}));
        QVERIFY(NiriHotkeyBackend::isNiriSession(QStringLiteral("Wayland"), QByteArrayLiteral("/run/user/1000/niri.sock")));
        QVERIFY(!NiriHotkeyBackend::isNiriSession(QStringLiteral("sway"), {}));
    }

    void requiresConfigIncludeSupport()
    {
        QVERIFY(NiriHotkeyBackend::supportsVersion(QStringLiteral("niri 25.11 (abc)")));
        QVERIFY(NiriHotkeyBackend::supportsVersion(QStringLiteral("niri 26.04")));
        QVERIFY(!NiriHotkeyBackend::supportsVersion(QStringLiteral("niri 25.08")));
        QVERIFY(!NiriHotkeyBackend::supportsVersion(QStringLiteral("unknown")));
    }

    void rendersSilentNonRepeatingToggleBinding()
    {
        Hotkey hotkey;
        hotkey.key = Qt::Key_F6;
        hotkey.modifiers = Qt::ControlModifier | Qt::ShiftModifier;
        const QString binding = NiriHotkeyBackend::renderBinding(
            hotkey, QStringLiteral("/opt/Omni Clicker/omniclicker"), QStringLiteral("Ctrl+Shift+F6"));
        QVERIFY(binding.contains(QStringLiteral("Ctrl+Shift+F6 repeat=false hotkey-overlay-title=null")));
        QVERIFY(binding.contains(QStringLiteral("spawn \"/opt/Omni Clicker/omniclicker\" \"--toggle-id\" \"Ctrl+Shift+F6\"")));
    }

    void generatedConfigPassesNiriValidation()
    {
        const QString niri = QStandardPaths::findExecutable(QStringLiteral("niri"));
        if (niri.isEmpty()) {
            QSKIP("Niri is not installed; generated KDL syntax cannot be validated here.");
        }
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Hotkey hotkey;
        hotkey.key = Qt::Key_F6;
        hotkey.modifiers = Qt::ControlModifier | Qt::ShiftModifier;
        const QString binding = NiriHotkeyBackend::renderBinding(
            hotkey, QStringLiteral("/usr/bin/omniclicker"), QStringLiteral("Ctrl+Shift+F6"));
        const QString fragmentPath = dir.filePath(QStringLiteral("hotkeys.kdl"));
        QFile fragment(fragmentPath);
        QVERIFY(fragment.open(QIODevice::WriteOnly));
        QVERIFY(fragment.write((QStringLiteral("binds {\n") + binding + QStringLiteral("\n}\n")).toUtf8()) > 0);
        fragment.close();

        const QString configPath = dir.filePath(QStringLiteral("config.kdl"));
        QFile config(configPath);
        QVERIFY(config.open(QIODevice::WriteOnly));
        QVERIFY(config.write((QStringLiteral("include \"") + fragmentPath + QStringLiteral("\"\n")).toUtf8()) > 0);
        config.close();

        QProcess validator;
        validator.start(niri, {QStringLiteral("validate"), QStringLiteral("--config"), configPath});
        QVERIFY(validator.waitForFinished(5000));
        QCOMPARE(validator.exitStatus(), QProcess::NormalExit);
        QCOMPARE(validator.exitCode(), 0);
    }

    void managesAndRemovesBindingsWithoutReplacingUserConfig()
    {
        const QString niri = QStandardPaths::findExecutable(QStringLiteral("niri"));
        if (niri.isEmpty()) {
            QSKIP("Niri is not installed; config lifecycle cannot be exercised.");
        }
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QByteArray previousNiriConfig = qgetenv("NIRI_CONFIG");
        const QByteArray previousConfigHome = qgetenv("XDG_CONFIG_HOME");
        const QString configPath = dir.filePath(QStringLiteral("custom-niri.kdl"));
        QFile config(configPath);
        QVERIFY(config.open(QIODevice::WriteOnly));
        const QByteArray original = QByteArrayLiteral("// Keep my config\nbinds { Mod+Q { close-window; } }\n");
        QCOMPARE(config.write(original), original.size());
        config.close();
        qputenv("NIRI_CONFIG", configPath.toLocal8Bit());
        qputenv("XDG_CONFIG_HOME", dir.path().toLocal8Bit());

        Hotkey hotkey;
        hotkey.key = Qt::Key_F6;
        hotkey.modifiers = Qt::ControlModifier | Qt::ShiftModifier;
        NiriHotkeyBackend backend;
        QString error;
        QVERIFY2(backend.start(hotkey, {}, QStringLiteral("Ctrl+Shift+F6"), &error), qPrintable(error));

        QFile updatedConfig(configPath);
        QVERIFY(updatedConfig.open(QIODevice::ReadOnly));
        const QByteArray configured = updatedConfig.readAll();
        QVERIFY(configured.startsWith(original));
        QVERIFY(configured.contains(QByteArrayLiteral("OmniClicker-managed global hotkeys")));

        QFile managed(dir.filePath(QStringLiteral("omniclicker/omniclicker-hotkeys.kdl")));
        QVERIFY(managed.open(QIODevice::ReadOnly));
        QVERIFY(managed.readAll().contains(QByteArrayLiteral("Ctrl+Shift+F6")));
        managed.close();

        backend.stop();
        QVERIFY(managed.open(QIODevice::ReadOnly));
        QVERIFY(managed.readAll().isEmpty());

        if (previousNiriConfig.isNull()) qputenv("NIRI_CONFIG", QByteArray());
        else qputenv("NIRI_CONFIG", previousNiriConfig);
        if (previousConfigHome.isNull()) qunsetenv("XDG_CONFIG_HOME");
        else qputenv("XDG_CONFIG_HOME", previousConfigHome);
    }
};

QTEST_GUILESS_MAIN(NiriHotkeyBackendTest)
#include "NiriHotkeyBackendTest.moc"
