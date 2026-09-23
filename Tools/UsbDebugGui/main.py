"""STM32 USB CDC debug monitor."""

from __future__ import annotations

import math
import struct
import sys
import time
from collections import deque
from datetime import datetime
from pathlib import Path

import serial
from serial.tools import list_ports

from PySide6.QtCore import QLineF, QRectF, QSettings, Qt, QTimer
from PySide6.QtGui import QBrush, QColor, QFont, QPainter, QPainterPath, QPen
from PySide6.QtWidgets import (
    QApplication,
    QComboBox,
    QDialog,
    QFrame,
    QFileDialog,
    QGridLayout,
    QHBoxLayout,
    QHeaderView,
    QLabel,
    QLineEdit,
    QMainWindow,
    QMessageBox,
    QPlainTextEdit,
    QPushButton,
    QSizePolicy,
    QSplitter,
    QStatusBar,
    QTabWidget,
    QTableWidget,
    QTableWidgetItem,
    QTreeWidget,
    QTreeWidgetItem,
    QVBoxLayout,
    QWidget,
)

from debug_protocol import (
    DebugProtocolError,
    MAX_DATA_SIZE,
    MULTI_MAX_DATA_SIZE,
    MULTI_MAX_ITEMS,
    read_memory,
    read_variables,
)
from elf_analysis import ElfSymbol, load_ram_symbols


APP_STYLE = """
QWidget {
    background: #10151d;
    color: #dce5f2;
    font-family: "Segoe UI";
    font-size: 10pt;
}
QMainWindow, QStatusBar {
    background: #0c1118;
}
QFrame#card {
    background: #161d27;
    border: 1px solid #263243;
    border-radius: 10px;
}
QFrame#headerCard {
    background: #131b26;
    border-bottom: 1px solid #263243;
}
QLabel#title {
    color: #f4f7fb;
    font-size: 20pt;
    font-weight: 700;
}
QLabel#sectionTitle {
    color: #f4f7fb;
    font-size: 12pt;
    font-weight: 650;
}
QLabel#muted {
    color: #8290a3;
}
QLabel#statusBadge {
    color: #ffabb0;
    background: #3a2028;
    border: 1px solid #64303b;
    border-radius: 10px;
    padding: 3px 9px;
    font-weight: 600;
}
QPushButton {
    background: #202a38;
    border: 1px solid #334156;
    border-radius: 6px;
    padding: 7px 13px;
    color: #dce5f2;
}
QPushButton:hover {
    background: #293547;
    border-color: #4a5d77;
}
QPushButton#primaryButton {
    background: #2f6fed;
    border-color: #4b82ef;
    color: white;
    font-weight: 650;
}
QPushButton#dangerButton {
    color: #ffb1b5;
    border-color: #6a3540;
}
QLineEdit, QComboBox, QPlainTextEdit {
    background: #0e141c;
    border: 1px solid #2b3749;
    border-radius: 6px;
    padding: 6px 8px;
    selection-background-color: #2f6fed;
}
QLineEdit:focus, QComboBox:focus, QPlainTextEdit:focus {
    border-color: #4b82ef;
}
QTreeWidget, QTableWidget {
    background: #111821;
    alternate-background-color: #141d28;
    border: 1px solid #263243;
    border-radius: 6px;
    gridline-color: #253142;
    selection-background-color: #244f91;
    selection-color: white;
}
QHeaderView::section {
    background: #1b2532;
    color: #9eacbe;
    border: 0;
    border-right: 1px solid #2a3647;
    border-bottom: 1px solid #2a3647;
    padding: 7px;
    font-weight: 600;
}
QTabWidget::pane {
    border: 1px solid #263243;
    border-radius: 8px;
    background: #111821;
    top: -1px;
}
QTabBar::tab {
    background: #141b25;
    color: #8795a8;
    padding: 9px 18px;
    border: 1px solid #263243;
    border-bottom: 0;
    margin-right: 3px;
    border-top-left-radius: 6px;
    border-top-right-radius: 6px;
}
QTabBar::tab:selected {
    background: #1c2633;
    color: #f4f7fb;
}
QSplitter::handle {
    background: #263243;
    width: 1px;
    height: 1px;
}
QStatusBar {
    color: #78869a;
    border-top: 1px solid #222d3b;
}
"""


class LiveGraphWidget(QFrame):
    """Draw the samples collected by one Watch entry."""

    def __init__(self, entry: dict) -> None:
        super().__init__()
        self.entry = entry
        self.range_seconds: float | None = 10.0
        self.setMinimumHeight(230)
        self.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)
        self.setObjectName("chartPreview")

    def paintEvent(self, event) -> None:  # noqa: N802 - Qt virtual method name
        super().paintEvent(event)
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)

        bounds = QRectF(self.rect()).adjusted(52, 24, -20, -38)
        painter.fillRect(self.rect(), QColor("#0e151e"))

        grid_pen = QPen(QColor("#263243"), 1)
        painter.setPen(grid_pen)
        for index in range(6):
            y = bounds.top() + bounds.height() * index / 5
            painter.drawLine(QLineF(bounds.left(), y, bounds.right(), y))
        for index in range(9):
            x = bounds.left() + bounds.width() * index / 8
            painter.drawLine(QLineF(x, bounds.top(), x, bounds.bottom()))

        painter.setPen(QPen(QColor("#748197"), 1))
        samples = list(self.entry["history"])
        if self.range_seconds is not None and samples:
            cutoff = samples[-1][0] - self.range_seconds
            samples = [sample for sample in samples if sample[0] >= cutoff]

        symbol: ElfSymbol = self.entry["symbol"]
        painter.drawText(
            QRectF(12, 7, self.width() - 24, 20),
            Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter,
            f"{symbol.name} · {symbol.type_name}",
        )
        painter.drawText(
            QRectF(0, self.height() - 30, self.width() - 18, 18),
            Qt.AlignmentFlag.AlignRight | Qt.AlignmentFlag.AlignVCenter,
            "시간 (ms)",
        )

        if not samples:
            painter.drawText(
                bounds,
                Qt.AlignmentFlag.AlignCenter,
                "수신된 숫자 데이터가 없습니다.",
            )
            return

        values = [sample[1] for sample in samples]
        minimum = min(values)
        maximum = max(values)
        if math.isclose(minimum, maximum):
            padding = max(abs(minimum) * 0.05, 1.0)
            minimum -= padding
            maximum += padding
        start_time = samples[0][0]
        duration = max(samples[-1][0] - start_time, 0.001)

        path = QPainterPath()
        for index, (sample_time, value) in enumerate(samples):
            x = bounds.left() + bounds.width() * (sample_time - start_time) / duration
            y = bounds.bottom() - bounds.height() * (value - minimum) / (maximum - minimum)
            if index == 0:
                path.moveTo(x, y)
            else:
                path.lineTo(x, y)

        painter.setPen(QPen(QColor("#54a2ff"), 2.2))
        painter.drawPath(path)
        painter.setPen(QPen(QColor("#9eacbe"), 1))
        painter.drawText(8, int(bounds.top() + 5), f"{maximum:.7g}")
        painter.drawText(8, int(bounds.bottom()), f"{minimum:.7g}")
        painter.drawText(
            QRectF(bounds.left(), bounds.top(), bounds.width() - 8, 20),
            Qt.AlignmentFlag.AlignRight,
            f"현재 {values[-1]:.7g}",
        )


class LiveGraphDialog(QDialog):
    """Independent live graph window for one Watch variable."""

    def __init__(self, entry: dict, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        symbol: ElfSymbol = entry["symbol"]
        self.setWindowTitle(f"실시간 그래프 · {symbol.name}")
        self.resize(760, 420)

        layout = QVBoxLayout(self)
        toolbar = QHBoxLayout()
        title = QLabel(symbol.name)
        title.setObjectName("sectionTitle")
        self.range_combo = QComboBox()
        self.range_combo.addItem("최근 10초", 10.0)
        self.range_combo.addItem("최근 30초", 30.0)
        self.range_combo.addItem("최근 1분", 60.0)
        self.range_combo.addItem("전체", None)
        toolbar.addWidget(title)
        toolbar.addStretch()
        toolbar.addWidget(self.range_combo)
        layout.addLayout(toolbar)

        self.graph = LiveGraphWidget(entry)
        self.range_combo.currentIndexChanged.connect(self._change_range)
        layout.addWidget(self.graph, 1)

        self.timer = QTimer(self)
        self.timer.setInterval(100)
        self.timer.timeout.connect(self.graph.update)
        self.timer.start()

    def _change_range(self) -> None:
        self.graph.range_seconds = self.range_combo.currentData()
        self.graph.update()


class UsbDebugWindow(QMainWindow):
    """ELF symbol browser and USB CDC memory reader."""

    def __init__(self) -> None:
        super().__init__()
        self.serial_port: serial.Serial | None = None
        self.elf_symbols: list[ElfSymbol] = []
        self.watch_entries: list[dict] = []
        self.watch_cursor = 0
        self.watch_paused = False
        self.rx_bytes = 0
        self.tx_bytes = 0
        self.settings = QSettings("Stm32_Test", "UsbDebugGui")
        self.setWindowTitle("STM32 USB Debug Monitor")
        self.resize(1480, 900)
        self.setMinimumSize(1120, 720)

        root = QWidget()
        root_layout = QVBoxLayout(root)
        root_layout.setContentsMargins(0, 0, 0, 0)
        root_layout.setSpacing(0)
        root_layout.addWidget(self._build_header())

        body = QWidget()
        body_layout = QVBoxLayout(body)
        body_layout.setContentsMargins(18, 16, 18, 14)
        body_layout.setSpacing(14)
        body_layout.addWidget(self._build_connection_panel())
        body_layout.addWidget(self._build_tabs(), 1)
        root_layout.addWidget(body, 1)

        self.setCentralWidget(root)
        self.setStatusBar(self._build_status_bar())

        self.watch_timer = QTimer(self)
        self.watch_timer.setInterval(20)
        self.watch_timer.timeout.connect(self._poll_watch)
        self.watch_timer.start()
        self._refresh_ports()
        QTimer.singleShot(0, self._restore_elf)

    def _build_header(self) -> QWidget:
        header = QFrame()
        header.setObjectName("headerCard")
        layout = QHBoxLayout(header)
        layout.setContentsMargins(22, 15, 22, 15)

        title_box = QVBoxLayout()
        title_box.setSpacing(2)
        title = QLabel("STM32 USB Debug Monitor")
        title.setObjectName("title")
        subtitle = QLabel("USB CDC · ELF Symbol Watch · Memory Inspector")
        subtitle.setObjectName("muted")
        title_box.addWidget(title)
        title_box.addWidget(subtitle)

        layout.addLayout(title_box)
        layout.addStretch()

        self.firmware_label = QLabel("Firmware: ELF 미선택")
        self.firmware_label.setObjectName("muted")
        self.status_badge = QLabel("●  DISCONNECTED")
        self.status_badge.setObjectName("statusBadge")
        layout.addWidget(self.firmware_label)
        layout.addSpacing(16)
        layout.addWidget(self.status_badge)
        return header

    def _build_connection_panel(self) -> QWidget:
        panel = QFrame()
        panel.setObjectName("card")
        layout = QHBoxLayout(panel)
        layout.setContentsMargins(16, 13, 16, 13)
        layout.setSpacing(10)

        layout.addWidget(self._label("USB 포트"))
        self.port_combo = QComboBox()
        self.port_combo.setMinimumWidth(280)
        layout.addWidget(self.port_combo)

        refresh_button = QPushButton("포트 새로고침")
        refresh_button.clicked.connect(self._refresh_ports)
        layout.addWidget(refresh_button)

        layout.addSpacing(12)
        layout.addWidget(self._label("Baud"))
        self.baud_combo = QComboBox()
        self.baud_combo.addItems(["115200", "230400", "460800", "921600"])
        self.baud_combo.setCurrentText("115200")
        self.baud_combo.setMinimumWidth(110)
        layout.addWidget(self.baud_combo)

        self.connect_button = QPushButton("연결")
        self.connect_button.setObjectName("primaryButton")
        self.connect_button.setMinimumWidth(90)
        self.connect_button.clicked.connect(self._toggle_connection)
        layout.addWidget(self.connect_button)

        layout.addStretch()
        self.packet_label = QLabel("RX  0 B    ·    TX  0 B")
        self.packet_label.setObjectName("muted")
        layout.addWidget(self.packet_label)
        return panel

    def _build_tabs(self) -> QTabWidget:
        tabs = QTabWidget()
        tabs.setDocumentMode(True)
        tabs.addTab(self._build_watch_page(), "변수 Watch")
        tabs.addTab(self._build_memory_page(), "메모리")
        tabs.addTab(self._build_log_page(), "통신 로그")
        return tabs

    def _build_watch_page(self) -> QWidget:
        page = QWidget()
        page_layout = QVBoxLayout(page)
        page_layout.setContentsMargins(12, 12, 12, 12)

        splitter = QSplitter(Qt.Orientation.Horizontal)
        splitter.addWidget(self._build_variable_browser())
        splitter.addWidget(self._build_watch_workspace())
        splitter.setSizes([430, 900])
        splitter.setStretchFactor(0, 0)
        splitter.setStretchFactor(1, 1)
        page_layout.addWidget(splitter)
        return page

    def _build_variable_browser(self) -> QWidget:
        panel = QFrame()
        panel.setObjectName("card")
        layout = QVBoxLayout(panel)
        layout.setContentsMargins(13, 13, 13, 13)
        layout.setSpacing(10)

        title_row = QHBoxLayout()
        title = QLabel("ELF 변수 탐색기")
        title.setObjectName("sectionTitle")
        self.symbol_count = QLabel("ELF 미선택")
        self.symbol_count.setObjectName("muted")
        title_row.addWidget(title)
        title_row.addStretch()
        title_row.addWidget(self.symbol_count)
        layout.addLayout(title_row)

        elf_row = QHBoxLayout()
        self.elf_path = QLineEdit()
        self.elf_path.setPlaceholderText("Debug/stm32f103c8t6.elf")
        self.elf_path.setReadOnly(True)
        elf_button = QPushButton("ELF 열기")
        elf_button.clicked.connect(self._open_elf)
        elf_row.addWidget(self.elf_path, 1)
        elf_row.addWidget(elf_button)
        layout.addLayout(elf_row)

        self.symbol_search = QLineEdit()
        self.symbol_search.setPlaceholderText("변수 이름, 크기 또는 주소 검색")
        self.symbol_search.textChanged.connect(self._filter_symbols)
        layout.addWidget(self.symbol_search)

        self.variable_tree = QTreeWidget()
        self.variable_tree.setHeaderLabels(["변수", "타입", "주소"])
        self.variable_tree.setAlternatingRowColors(True)
        self.variable_tree.setRootIsDecorated(False)
        self.variable_tree.setUniformRowHeights(True)
        self.variable_tree.itemClicked.connect(self._activate_variable_item)
        self.variable_tree.header().setSectionResizeMode(0, QHeaderView.ResizeMode.Stretch)
        self.variable_tree.header().setSectionResizeMode(1, QHeaderView.ResizeMode.ResizeToContents)
        self.variable_tree.header().setSectionResizeMode(2, QHeaderView.ResizeMode.ResizeToContents)

        layout.addWidget(self.variable_tree, 1)

        footer = QHBoxLayout()
        footer.addWidget(self._label("클릭 시 Watch 주기"))
        self.watch_period = QComboBox()
        self.watch_period.addItems(["10 ms", "100 ms", "1000 ms"])
        self.watch_period.setCurrentText("100 ms")
        footer.addWidget(self.watch_period)
        footer.addStretch()
        layout.addLayout(footer)
        return panel

    def _build_watch_workspace(self) -> QWidget:
        workspace = QWidget()
        layout = QVBoxLayout(workspace)
        layout.setContentsMargins(12, 0, 0, 0)
        layout.setSpacing(12)

        table_panel = QFrame()
        table_panel.setObjectName("card")
        table_layout = QVBoxLayout(table_panel)
        table_layout.setContentsMargins(13, 13, 13, 13)

        table_header = QHBoxLayout()
        title = QLabel("활성 Watch")
        title.setObjectName("sectionTitle")
        self.pause_button = QPushButton("일시정지")
        clear_button = QPushButton("전체 해제")
        clear_button.setObjectName("dangerButton")
        self.pause_button.clicked.connect(self._toggle_watch_pause)
        clear_button.clicked.connect(self._clear_watches)
        table_header.addWidget(title)
        table_header.addStretch()
        table_header.addWidget(self.pause_button)
        table_header.addWidget(clear_button)
        table_layout.addLayout(table_header)

        self.watch_table = QTableWidget(0, 8)
        self.watch_table.setHorizontalHeaderLabels(
            ["그래프", "변수", "현재값", "타입", "표시", "주기", "최종 Tick", "상태"]
        )
        self.watch_table.setAlternatingRowColors(True)
        self.watch_table.setSelectionBehavior(QTableWidget.SelectionBehavior.SelectRows)
        self.watch_table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)
        self.watch_table.verticalHeader().setVisible(False)

        header = self.watch_table.horizontalHeader()
        header.setSectionResizeMode(0, QHeaderView.ResizeMode.ResizeToContents)
        header.setSectionResizeMode(1, QHeaderView.ResizeMode.Stretch)
        for column in range(2, 8):
            header.setSectionResizeMode(column, QHeaderView.ResizeMode.ResizeToContents)
        table_layout.addWidget(self.watch_table)
        layout.addWidget(table_panel, 1)
        return workspace

    def _build_memory_page(self) -> QWidget:
        page = QWidget()
        layout = QVBoxLayout(page)
        layout.setContentsMargins(12, 12, 12, 12)
        layout.setSpacing(12)

        controls = QFrame()
        controls.setObjectName("card")
        control_layout = QGridLayout(controls)
        control_layout.setContentsMargins(14, 14, 14, 14)
        control_layout.setHorizontalSpacing(10)

        control_layout.addWidget(self._label("영역"), 0, 0)
        region_combo = QComboBox()
        region_combo.addItems(["SRAM", "Flash", "사용자 지정"])
        control_layout.addWidget(region_combo, 0, 1)
        control_layout.addWidget(self._label("시작 주소"), 0, 2)
        self.memory_address = QLineEdit("0x20000000")
        control_layout.addWidget(self.memory_address, 0, 3)
        control_layout.addWidget(self._label("길이"), 0, 4)
        self.memory_length = QComboBox()
        self.memory_length.addItems(["8 bytes", "16 bytes", "64 bytes", "128 bytes", "256 bytes"])
        self.memory_length.setCurrentText("64 bytes")
        control_layout.addWidget(self.memory_length, 0, 5)
        read_button = QPushButton("메모리 읽기")
        read_button.setObjectName("primaryButton")
        read_button.clicked.connect(self._read_memory_page)
        control_layout.addWidget(read_button, 0, 6)
        save_button = QPushButton("덤프 저장")
        control_layout.addWidget(save_button, 0, 7)
        control_layout.setColumnStretch(3, 1)
        layout.addWidget(controls)

        viewer = QFrame()
        viewer.setObjectName("card")
        viewer_layout = QVBoxLayout(viewer)
        viewer_layout.setContentsMargins(13, 13, 13, 13)

        title = QLabel("Hex Viewer")
        title.setObjectName("sectionTitle")
        viewer_layout.addWidget(title)

        self.hex_view = QPlainTextEdit()
        self.hex_view.setReadOnly(True)
        self.hex_view.setFont(QFont("Cascadia Mono", 10))
        self.hex_view.setPlainText(
            "Address      00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F   ASCII\n"
            "────────────────────────────────────────────────────────────────────────────\n"
            "20000000     -- -- -- -- -- -- -- --  -- -- -- -- -- -- -- --   ................\n"
            "20000010     -- -- -- -- -- -- -- --  -- -- -- -- -- -- -- --   ................\n"
            "20000020     -- -- -- -- -- -- -- --  -- -- -- -- -- -- -- --   ................\n"
            "20000030     -- -- -- -- -- -- -- --  -- -- -- -- -- -- -- --   ................"
        )
        viewer_layout.addWidget(self.hex_view, 1)
        layout.addWidget(viewer, 1)
        return page

    def _build_log_page(self) -> QWidget:
        page = QWidget()
        layout = QVBoxLayout(page)
        layout.setContentsMargins(12, 12, 12, 12)
        layout.setSpacing(10)

        toolbar = QHBoxLayout()
        title = QLabel("USB CDC 통신 로그")
        title.setObjectName("sectionTitle")
        direction = QComboBox()
        direction.addItems(["RX + TX", "RX만", "TX만", "오류만"])
        clear_button = QPushButton("로그 지우기")
        clear_button.clicked.connect(lambda: self.log_view.clear())
        save_button = QPushButton("로그 저장")
        toolbar.addWidget(title)
        toolbar.addStretch()
        toolbar.addWidget(direction)
        toolbar.addWidget(clear_button)
        toolbar.addWidget(save_button)
        layout.addLayout(toolbar)

        self.log_view = QPlainTextEdit()
        self.log_view.setReadOnly(True)
        self.log_view.setFont(QFont("Cascadia Mono", 10))
        self.log_view.document().setMaximumBlockCount(5000)
        self.log_view.setPlainText("[--:--:--.---]  INFO  GUI 준비 완료")
        layout.addWidget(self.log_view, 1)
        return page

    def _build_status_bar(self) -> QStatusBar:
        status = QStatusBar()
        status.showMessage("준비 · ELF를 열고 USB CDC 포트에 연결하세요")
        version = QLabel("UI v0.2")
        version.setObjectName("muted")
        status.addPermanentWidget(version)
        return status

    def _refresh_ports(self) -> None:
        previous = self.port_combo.currentData()
        ports_by_device = {}
        for port in list_ports.comports():
            ports_by_device.setdefault(port.device.upper(), port)

        self.port_combo.clear()
        self.port_combo.addItem("COM 포트를 선택하세요", None)
        preferred_index = 0
        for port in sorted(ports_by_device.values(), key=lambda item: item.device):
            description = port.description or "Serial device"
            self.port_combo.addItem(f"{port.device} — {description}", port.device)
            index = self.port_combo.count() - 1
            if port.device == previous:
                preferred_index = index
            elif previous is None and port.vid == 0x0483 and port.pid == 0x5740:
                preferred_index = index
        self.port_combo.setCurrentIndex(preferred_index)

    def _toggle_connection(self) -> None:
        if self.serial_port is not None and self.serial_port.is_open:
            self._disconnect_serial("사용자가 연결을 해제했습니다.")
            return

        device = self.port_combo.currentData()
        if not device:
            self._show_error("먼저 USB CDC COM 포트를 선택하세요.")
            return

        try:
            self.serial_port = serial.Serial(
                device,
                baudrate=int(self.baud_combo.currentText()),
                timeout=0.05,
                write_timeout=0.5,
            )
            self.serial_port.reset_input_buffer()
        except serial.SerialException as error:
            self.serial_port = None
            self._show_error(f"{device} 연결 실패: {error}")
            return

        self.connect_button.setText("연결 해제")
        self.status_badge.setText("●  CONNECTED")
        self.status_badge.setStyleSheet(
            "color:#8ff0c0; background:#17352c; border:1px solid #28644f;"
            "border-radius:10px; padding:3px 9px; font-weight:600;"
        )
        self.statusBar().showMessage(f"연결됨 · {device}")
        self._log("INFO", f"{device} 연결, {self.baud_combo.currentText()} baud")

    def _disconnect_serial(self, reason: str) -> None:
        if self.serial_port is not None:
            try:
                self.serial_port.close()
            except serial.SerialException:
                pass
        self.serial_port = None
        self.connect_button.setText("연결")
        self.status_badge.setText("●  DISCONNECTED")
        self.status_badge.setStyleSheet("")
        self.statusBar().showMessage("연결 해제됨")
        self._log("INFO", reason)

    def _open_elf(self) -> None:
        remembered = self.settings.value("last_elf_path", "", type=str)
        default_elf = Path(__file__).resolve().parents[2] / "Debug" / "stm32f103c8t6.elf"
        remembered_path = Path(remembered) if remembered else default_elf
        initial = str(
            remembered_path if remembered_path.exists() else remembered_path.parent
        )
        selected, _ = QFileDialog.getOpenFileName(
            self, "STM32 ELF 열기", initial, "ELF files (*.elf *.out);;All files (*)"
        )
        if not selected:
            return
        self._load_elf(Path(selected))

    def _restore_elf(self) -> None:
        remembered = self.settings.value("last_elf_path", "", type=str)
        if not remembered:
            return
        path = Path(remembered)
        if path.is_file():
            self._load_elf(path, automatic=True)

    def _load_elf(self, path: Path, automatic: bool = False) -> None:
        try:
            symbols = load_ram_symbols(path)
        except (OSError, RuntimeError, ValueError) as error:
            message = f"ELF 분석 실패: {error}"
            if automatic:
                self._log("ERROR", message)
                self.statusBar().showMessage(message)
            else:
                self._show_error(message)
            return

        self._clear_watches()
        self.elf_symbols = symbols
        self.elf_path.setText(str(path))
        self.variable_tree.clear()
        for symbol in symbols:
            item = QTreeWidgetItem(
                [symbol.name, symbol.type_name, f"0x{symbol.address:08X}"]
            )
            item.setData(0, Qt.ItemDataRole.UserRole, symbol)
            self.variable_tree.addTopLevelItem(item)

        self.settings.setValue("last_elf_path", str(path.resolve()))
        self.settings.sync()
        self.symbol_count.setText(f"RAM 변수 {len(symbols)}개")
        self.firmware_label.setText(f"Firmware: {path.name}")
        self.statusBar().showMessage(f"ELF 로드됨 · RAM 변수 {len(symbols)}개")
        prefix = "ELF 자동 로드" if automatic else "ELF 로드"
        self._log("INFO", f"{prefix} {path} · RAM 심볼 {len(symbols)}개")

    def _filter_symbols(self, text: str) -> None:
        query = text.strip().lower()
        for index in range(self.variable_tree.topLevelItemCount()):
            item = self.variable_tree.topLevelItem(index)
            haystack = " ".join(item.text(column) for column in range(3)).lower()
            item.setHidden(query not in haystack)

    def _activate_variable_item(self, item: QTreeWidgetItem, _column: int) -> None:
        symbol = item.data(0, Qt.ItemDataRole.UserRole)
        if symbol is None:
            return
        self._add_watch(symbol)

    def _add_watch(self, symbol: ElfSymbol) -> None:
        period_ms = int(self.watch_period.currentText().split()[0])
        existing = {(entry["symbol"].name, entry["symbol"].address) for entry in self.watch_entries}
        key = (symbol.name, symbol.address)
        if key in existing:
            return

        colors = ["#54a2ff", "#53d8a1", "#ffcb66", "#c994ff"]
        entry = {
            "symbol": symbol,
            "period_ms": period_ms,
            "next_due": 0.0,
            "last_data": b"",
            "display_mode": "typed",
            "history": deque(maxlen=6000),
            "graph_dialog": None,
        }
        self.watch_entries.append(entry)
        row = self.watch_table.rowCount()
        self.watch_table.insertRow(row)

        graph_button = QPushButton("열기")
        graph_button.setToolTip(f"{symbol.name} 실시간 그래프 열기")
        graph_button.clicked.connect(lambda _checked=False, watch=entry: self._open_graph(watch))
        self.watch_table.setCellWidget(row, 0, graph_button)

        display_combo = QComboBox()
        display_combo.addItem("타입 값", "typed")
        display_combo.addItem("RAW 값", "raw")
        display_combo.currentIndexChanged.connect(
            lambda _index, watch=entry: self._change_watch_display(watch)
        )
        self.watch_table.setCellWidget(row, 4, display_combo)

        values = {
            1: symbol.name,
            2: "--",
            3: symbol.type_name,
            5: f"{period_ms} ms",
            6: "--",
            7: "대기",
        }
        for column, value in values.items():
            cell = QTableWidgetItem(value)
            if column == 1:
                cell.setForeground(QColor(colors[row % len(colors)]))
            if column in (2, 5, 6, 7):
                cell.setTextAlignment(Qt.AlignmentFlag.AlignCenter)
            self.watch_table.setItem(row, column, cell)

        self._set_symbol_active(symbol, True)
        self.statusBar().showMessage(f"Watch 활성화 · {symbol.name}")

    def _set_symbol_active(self, symbol: ElfSymbol, active: bool) -> None:
        for index in range(self.variable_tree.topLevelItemCount()):
            item = self.variable_tree.topLevelItem(index)
            candidate = item.data(0, Qt.ItemDataRole.UserRole)
            if candidate is None or candidate.name != symbol.name or candidate.address != symbol.address:
                continue
            for column in range(3):
                if active:
                    item.setBackground(column, QBrush(QColor("#183a31")))
                    item.setForeground(column, QBrush(QColor("#8ff0c0")))
                else:
                    item.setData(column, Qt.ItemDataRole.BackgroundRole, None)
                    item.setData(column, Qt.ItemDataRole.ForegroundRole, None)
            return

    def _toggle_watch_pause(self) -> None:
        self.watch_paused = not self.watch_paused
        self.pause_button.setText("재개" if self.watch_paused else "일시정지")
        self.statusBar().showMessage("Watch 일시정지" if self.watch_paused else "Watch 재개")

    def _clear_watches(self) -> None:
        for entry in self.watch_entries:
            dialog = entry.get("graph_dialog")
            if dialog is not None:
                dialog.close()
            self._set_symbol_active(entry["symbol"], False)
        self.watch_entries.clear()
        self.watch_cursor = 0
        self.watch_table.setRowCount(0)

    def _poll_watch(self) -> None:
        if self.watch_paused or not self._serial_is_open():
            return

        now = time.monotonic()
        entry_count = len(self.watch_entries)
        if entry_count == 0:
            return

        selected_rows = []
        targets = []
        total_size = 0
        for offset in range(entry_count):
            row = (self.watch_cursor + offset) % entry_count
            entry = self.watch_entries[row]
            if now < entry["next_due"]:
                continue
            symbol: ElfSymbol = entry["symbol"]
            read_size = min(symbol.size, MAX_DATA_SIZE)
            if ((len(targets) >= MULTI_MAX_ITEMS)
                or ((total_size + read_size) > MULTI_MAX_DATA_SIZE)):
                continue
            selected_rows.append(row)
            targets.append((symbol.address, read_size))
            total_size += read_size

        if not targets:
            return

        for row in selected_rows:
            entry = self.watch_entries[row]
            entry["next_due"] = now + entry["period_ms"] / 1000.0
        self.watch_cursor = (selected_rows[-1] + 1) % entry_count

        try:
            values = self._read_target_variables(targets)
            received_tick = str(int(time.monotonic() * 1000))
            for row, data in zip(selected_rows, values):
                entry = self.watch_entries[row]
                symbol = entry["symbol"]
                entry["last_data"] = data
                numeric_value = self._numeric_watch_value(data, symbol)
                if numeric_value is not None and math.isfinite(numeric_value):
                    entry["history"].append((now, numeric_value))
                value = self._format_watch_value(
                    data, symbol, entry["display_mode"]
                )
                self.watch_table.item(row, 2).setText(value)
                self.watch_table.item(row, 6).setText(received_tick)
                self.watch_table.item(row, 7).setText("정상")
        except (serial.SerialException, DebugProtocolError, ValueError) as error:
            error_text = str(error)
            for row in selected_rows:
                status_item = self.watch_table.item(row, 7)
                status_item.setText(
                    "시간 초과" if "시간 초과" in error_text else "응답 오류"
                )
                status_item.setToolTip(error_text)
            names = ", ".join(
                self.watch_entries[row]["symbol"].name for row in selected_rows
            )
            self._log("ERROR", f"다중 Watch [{names}]: {error}")
            self.watch_paused = True
            self.pause_button.setText("재개")
            self.statusBar().showMessage("통신 오류로 Watch가 일시정지되었습니다.")

    def _read_memory_page(self) -> None:
        try:
            address = int(self.memory_address.text().strip(), 0)
            length = int(self.memory_length.currentText().split()[0])
            data = self._read_target_memory(address, length)
        except (serial.SerialException, DebugProtocolError, ValueError) as error:
            self._show_error(f"메모리 읽기 실패: {error}")
            return
        self.hex_view.setPlainText(self._format_hex_dump(address, data))
        self.statusBar().showMessage(f"0x{address:08X}에서 {len(data)}바이트 읽음")

    def _read_target_memory(self, address: int, length: int) -> bytes:
        if not self._serial_is_open():
            raise DebugProtocolError("USB CDC가 연결되지 않았습니다.")
        return read_memory(self.serial_port, address, length, self._record_exchange)

    def _read_target_variables(self, items: list[tuple[int, int]]) -> list[bytes]:
        if not self._serial_is_open():
            raise DebugProtocolError("USB CDC가 연결되지 않았습니다.")
        return read_variables(self.serial_port, items, self._record_exchange)

    def _record_exchange(self, request: bytes, response) -> None:
        self.tx_bytes += len(request)
        self.rx_bytes += len(response.raw)
        self._log("TX", " ".join(f"{byte:02X}" for byte in request))
        self._log(
            "RX",
            " ".join(f"{byte:02X}" for byte in response.raw)
            + f"  (seq={response.sequence}, status={response.status})",
        )
        self._update_packet_label()

    def _serial_is_open(self) -> bool:
        return self.serial_port is not None and self.serial_port.is_open

    def _update_packet_label(self) -> None:
        self.packet_label.setText(f"RX  {self.rx_bytes} B    ·    TX  {self.tx_bytes} B")

    def _change_watch_display(self, entry: dict) -> None:
        try:
            row = self.watch_entries.index(entry)
        except ValueError:
            return
        combo = self.watch_table.cellWidget(row, 4)
        entry["display_mode"] = combo.currentData()
        data = entry.get("last_data", b"")
        if data:
            self.watch_table.item(row, 2).setText(
                self._format_watch_value(
                    data, entry["symbol"], entry["display_mode"]
                )
            )

    def _open_graph(self, entry: dict) -> None:
        if entry not in self.watch_entries:
            return
        dialog = entry.get("graph_dialog")
        if dialog is None:
            dialog = LiveGraphDialog(entry, self)
            entry["graph_dialog"] = dialog
        dialog.show()
        dialog.raise_()
        dialog.activateWindow()

    @classmethod
    def _numeric_watch_value(cls, data: bytes, symbol: ElfSymbol) -> float | None:
        if not data:
            return None
        kind = symbol.value_kind
        value_data = data
        if kind == "array":
            if symbol.element_size <= 0 or len(data) < symbol.element_size:
                return None
            value_data = data[:symbol.element_size]
            kind = symbol.element_kind
        if kind == "float" and len(value_data) in (4, 8):
            format_code = "<f" if len(value_data) == 4 else "<d"
            return float(struct.unpack(format_code, value_data)[0])
        if kind == "signed":
            return float(int.from_bytes(value_data, byteorder="little", signed=True))
        if kind in {"unsigned", "pointer", "bool", "raw"}:
            return float(int.from_bytes(value_data, byteorder="little", signed=False))
        return None

    def _format_watch_value(
        self, data: bytes, symbol: ElfSymbol, display_mode: str = "typed"
    ) -> str:
        if display_mode == "raw":
            return self._format_raw_value(data, symbol.size)
        return self._format_typed_value(data, symbol)

    @staticmethod
    def _format_raw_value(data: bytes, symbol_size: int) -> str:
        suffix = f" (앞 {len(data)}/{symbol_size} B)" \
            if symbol_size > len(data) else ""
        value = int.from_bytes(data, byteorder="little", signed=False)
        return f"0x{value:0{len(data) * 2}X} ({value}){suffix}"

    @classmethod
    def _format_typed_value(cls, data: bytes, symbol: ElfSymbol) -> str:
        suffix = f" (앞 {len(data)}/{symbol.size} B)" \
            if symbol.size > len(data) else ""

        if symbol.value_kind == "array" and symbol.element_size > 0:
            values = []
            for offset in range(0, len(data), symbol.element_size):
                element = data[offset : offset + symbol.element_size]
                if len(element) != symbol.element_size:
                    break
                values.append(cls._decode_scalar(element, symbol.element_kind))
            return "[" + ", ".join(values) + "]" + suffix

        if symbol.value_kind == "float" and len(data) in (4, 8):
            format_code = "<f" if len(data) == 4 else "<d"
            value = struct.unpack(format_code, data)[0]
            raw = int.from_bytes(data, byteorder="little", signed=False)
            return f"{value:.7g} (0x{raw:0{len(data) * 2}X}){suffix}"

        if symbol.value_kind == "signed":
            value = int.from_bytes(data, byteorder="little", signed=True)
            raw = int.from_bytes(data, byteorder="little", signed=False)
            return f"{value} (0x{raw:0{len(data) * 2}X}){suffix}"

        if symbol.value_kind == "unsigned":
            value = int.from_bytes(data, byteorder="little", signed=False)
            return f"{value} (0x{value:0{len(data) * 2}X}){suffix}"

        if symbol.value_kind == "bool":
            return ("true" if any(data) else "false") + suffix

        if symbol.value_kind == "pointer":
            value = int.from_bytes(data, byteorder="little", signed=False)
            return f"0x{value:0{len(data) * 2}X}{suffix}"

        return cls._format_raw_value(data, symbol.size)

    @staticmethod
    def _decode_scalar(data: bytes, kind: str) -> str:
        if kind == "float" and len(data) in (4, 8):
            format_code = "<f" if len(data) == 4 else "<d"
            return f"{struct.unpack(format_code, data)[0]:.7g}"
        if kind == "signed":
            return str(int.from_bytes(data, byteorder="little", signed=True))
        if kind == "bool":
            return "true" if any(data) else "false"
        if kind in {"unsigned", "pointer"}:
            return str(int.from_bytes(data, byteorder="little", signed=False))
        return "0x" + data[::-1].hex().upper()

    @staticmethod
    def _format_hex_dump(address: int, data: bytes) -> str:
        lines = ["Address      00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F   ASCII"]
        for offset in range(0, len(data), 16):
            chunk = data[offset : offset + 16]
            left = " ".join(f"{byte:02X}" for byte in chunk[:8])
            right = " ".join(f"{byte:02X}" for byte in chunk[8:])
            hex_text = f"{left:<23}  {right:<23}"
            ascii_text = "".join(chr(byte) if 32 <= byte < 127 else "." for byte in chunk)
            lines.append(f"{address + offset:08X}     {hex_text}   {ascii_text}")
        return "\n".join(lines)

    def _log(self, level: str, message: str) -> None:
        timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        self.log_view.appendPlainText(f"[{timestamp}]  {level:<5} {message}")

    def _show_error(self, message: str) -> None:
        self._log("ERROR", message)
        self.statusBar().showMessage(message)
        QMessageBox.warning(self, "STM32 USB Debug", message)

    def closeEvent(self, event) -> None:  # noqa: N802 - Qt virtual method name
        if self.serial_port is not None:
            self.serial_port.close()
        event.accept()

    @staticmethod
    def _label(text: str) -> QLabel:
        label = QLabel(text)
        label.setObjectName("muted")
        return label


def main() -> int:
    app = QApplication(sys.argv)
    app.setApplicationName("STM32 USB Debug Monitor")
    app.setOrganizationName("Stm32_Test")
    app.setStyle("Fusion")
    app.setStyleSheet(APP_STYLE)

    window = UsbDebugWindow()
    window.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
