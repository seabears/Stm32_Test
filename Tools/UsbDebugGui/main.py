"""STM32 USB Debug GUI layout prototype.

This first version intentionally contains no serial communication or button
event handlers.  It defines the desktop layout that later modules can connect
to pyserial, ELF/DWARF parsing, and the target debug protocol.
"""

from __future__ import annotations

import sys

from PySide6.QtCore import QLineF, QRectF, Qt
from PySide6.QtGui import QColor, QFont, QPainter, QPainterPath, QPen
from PySide6.QtWidgets import (
    QApplication,
    QComboBox,
    QFrame,
    QGridLayout,
    QHBoxLayout,
    QHeaderView,
    QLabel,
    QLineEdit,
    QMainWindow,
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


class TrendPreview(QFrame):
    """Static chart placeholder used before live samples are implemented."""

    def __init__(self) -> None:
        super().__init__()
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
        painter.drawText(
            QRectF(12, 7, self.width() - 24, 20),
            Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter,
            "실시간 트렌드 미리보기",
        )
        painter.drawText(
            QRectF(0, self.height() - 30, self.width() - 18, 18),
            Qt.AlignmentFlag.AlignRight | Qt.AlignmentFlag.AlignVCenter,
            "시간 (ms)",
        )

        points = [0.72, 0.60, 0.63, 0.42, 0.46, 0.31, 0.38, 0.22, 0.29]
        path = QPainterPath()
        for index, value in enumerate(points):
            x = bounds.left() + bounds.width() * index / (len(points) - 1)
            y = bounds.top() + bounds.height() * value
            if index == 0:
                path.moveTo(x, y)
            else:
                path.lineTo(x, y)

        painter.setPen(QPen(QColor("#54a2ff"), 2.2))
        painter.drawPath(path)


class UsbDebugWindow(QMainWindow):
    """Main UI shell. Widgets are intentionally not connected to handlers."""

    def __init__(self) -> None:
        super().__init__()
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

        firmware = QLabel("Firmware: 연결되지 않음")
        firmware.setObjectName("muted")
        status = QLabel("●  DISCONNECTED")
        status.setObjectName("statusBadge")
        layout.addWidget(firmware)
        layout.addSpacing(16)
        layout.addWidget(status)
        return header

    def _build_connection_panel(self) -> QWidget:
        panel = QFrame()
        panel.setObjectName("card")
        layout = QHBoxLayout(panel)
        layout.setContentsMargins(16, 13, 16, 13)
        layout.setSpacing(10)

        layout.addWidget(self._label("USB 포트"))
        port_combo = QComboBox()
        port_combo.addItems(["COM 포트를 선택하세요", "COM3", "COM5"])
        port_combo.setMinimumWidth(210)
        layout.addWidget(port_combo)

        refresh_button = QPushButton("포트 새로고침")
        layout.addWidget(refresh_button)

        layout.addSpacing(12)
        layout.addWidget(self._label("Baud"))
        baud_combo = QComboBox()
        baud_combo.addItems(["115200", "230400", "460800", "921600"])
        baud_combo.setCurrentText("115200")
        baud_combo.setMinimumWidth(110)
        layout.addWidget(baud_combo)

        connect_button = QPushButton("연결")
        connect_button.setObjectName("primaryButton")
        connect_button.setMinimumWidth(90)
        layout.addWidget(connect_button)

        layout.addStretch()
        packet_label = QLabel("RX  0 B    ·    TX  0 B    ·    0 pkt/s")
        packet_label.setObjectName("muted")
        layout.addWidget(packet_label)
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
        count = QLabel("예시 6개")
        count.setObjectName("muted")
        title_row.addWidget(title)
        title_row.addStretch()
        title_row.addWidget(count)
        layout.addLayout(title_row)

        elf_row = QHBoxLayout()
        elf_path = QLineEdit()
        elf_path.setPlaceholderText("Debug/stm32f103c8t6.elf")
        elf_path.setReadOnly(True)
        elf_button = QPushButton("ELF 열기")
        elf_row.addWidget(elf_path, 1)
        elf_row.addWidget(elf_button)
        layout.addLayout(elf_row)

        search = QLineEdit()
        search.setPlaceholderText("변수 이름, 타입 또는 주소 검색")
        layout.addWidget(search)

        variable_tree = QTreeWidget()
        variable_tree.setHeaderLabels(["변수", "타입", "주소"])
        variable_tree.setAlternatingRowColors(True)
        variable_tree.setRootIsDecorated(False)
        variable_tree.setUniformRowHeights(True)
        variable_tree.header().setSectionResizeMode(0, QHeaderView.ResizeMode.Stretch)
        variable_tree.header().setSectionResizeMode(1, QHeaderView.ResizeMode.ResizeToContents)
        variable_tree.header().setSectionResizeMode(2, QHeaderView.ResizeMode.ResizeToContents)

        sample_variables = [
            ("motorPwm", "uint16_t", "0x20000020"),
            ("encoderCount", "int32_t", "0x20000024"),
            ("batteryMv", "uint16_t", "0x20000028"),
            ("targetSpeed", "int32_t", "0x2000002C"),
            ("SystemCoreClock", "uint32_t", "0x20000030"),
            ("uwTick", "uint32_t", "0x20000034"),
        ]
        for name, type_name, address in sample_variables:
            item = QTreeWidgetItem([name, type_name, address])
            item.setCheckState(0, Qt.CheckState.Unchecked)
            variable_tree.addTopLevelItem(item)

        layout.addWidget(variable_tree, 1)

        footer = QHBoxLayout()
        period = QComboBox()
        period.addItems(["1 ms", "10 ms", "100 ms", "1000 ms"])
        period.setCurrentText("100 ms")
        add_button = QPushButton("선택 변수 Watch 추가")
        add_button.setObjectName("primaryButton")
        footer.addWidget(period)
        footer.addWidget(add_button, 1)
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
        pause_button = QPushButton("일시정지")
        clear_button = QPushButton("전체 해제")
        clear_button.setObjectName("dangerButton")
        table_header.addWidget(title)
        table_header.addStretch()
        table_header.addWidget(pause_button)
        table_header.addWidget(clear_button)
        table_layout.addLayout(table_header)

        watch_table = QTableWidget(4, 7)
        watch_table.setHorizontalHeaderLabels(
            ["그래프", "변수", "현재값", "타입", "주기", "최종 Tick", "상태"]
        )
        watch_table.setAlternatingRowColors(True)
        watch_table.setSelectionBehavior(QTableWidget.SelectionBehavior.SelectRows)
        watch_table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)
        watch_table.verticalHeader().setVisible(False)

        watch_rows = [
            ("●", "motorPwm", "500", "uint16_t", "10 ms", "152300", "대기"),
            ("●", "encoderCount", "13,542", "int32_t", "1 ms", "152301", "대기"),
            ("●", "batteryMv", "12,150", "uint16_t", "100 ms", "152200", "대기"),
            ("●", "targetSpeed", "1200", "int32_t", "100 ms", "152200", "대기"),
        ]
        colors = ["#54a2ff", "#53d8a1", "#ffcb66", "#c994ff"]
        for row, values in enumerate(watch_rows):
            for column, value in enumerate(values):
                item = QTableWidgetItem(value)
                if column == 0:
                    item.setForeground(QColor(colors[row]))
                    item.setTextAlignment(Qt.AlignmentFlag.AlignCenter)
                elif column in (2, 4, 5, 6):
                    item.setTextAlignment(Qt.AlignmentFlag.AlignCenter)
                watch_table.setItem(row, column, item)

        header = watch_table.horizontalHeader()
        header.setSectionResizeMode(0, QHeaderView.ResizeMode.ResizeToContents)
        header.setSectionResizeMode(1, QHeaderView.ResizeMode.Stretch)
        for column in range(2, 7):
            header.setSectionResizeMode(column, QHeaderView.ResizeMode.ResizeToContents)
        table_layout.addWidget(watch_table)
        layout.addWidget(table_panel, 3)

        chart_panel = QFrame()
        chart_panel.setObjectName("card")
        chart_layout = QVBoxLayout(chart_panel)
        chart_layout.setContentsMargins(13, 13, 13, 13)

        chart_header = QHBoxLayout()
        chart_title = QLabel("실시간 그래프")
        chart_title.setObjectName("sectionTitle")
        range_combo = QComboBox()
        range_combo.addItems(["최근 10초", "최근 30초", "최근 1분", "전체"])
        export_button = QPushButton("CSV 내보내기")
        chart_header.addWidget(chart_title)
        chart_header.addStretch()
        chart_header.addWidget(range_combo)
        chart_header.addWidget(export_button)
        chart_layout.addLayout(chart_header)
        chart_layout.addWidget(TrendPreview(), 1)
        layout.addWidget(chart_panel, 4)
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
        address = QLineEdit("0x20000000")
        control_layout.addWidget(address, 0, 3)
        control_layout.addWidget(self._label("길이"), 0, 4)
        length = QComboBox()
        length.addItems(["64 bytes", "128 bytes", "256 bytes", "512 bytes"])
        control_layout.addWidget(length, 0, 5)
        read_button = QPushButton("메모리 읽기")
        read_button.setObjectName("primaryButton")
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

        hex_view = QPlainTextEdit()
        hex_view.setReadOnly(True)
        hex_view.setFont(QFont("Cascadia Mono", 10))
        hex_view.setPlainText(
            "Address      00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F   ASCII\n"
            "────────────────────────────────────────────────────────────────────────────\n"
            "20000000     -- -- -- -- -- -- -- --  -- -- -- -- -- -- -- --   ................\n"
            "20000010     -- -- -- -- -- -- -- --  -- -- -- -- -- -- -- --   ................\n"
            "20000020     -- -- -- -- -- -- -- --  -- -- -- -- -- -- -- --   ................\n"
            "20000030     -- -- -- -- -- -- -- --  -- -- -- -- -- -- -- --   ................"
        )
        viewer_layout.addWidget(hex_view, 1)
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
        save_button = QPushButton("로그 저장")
        toolbar.addWidget(title)
        toolbar.addStretch()
        toolbar.addWidget(direction)
        toolbar.addWidget(clear_button)
        toolbar.addWidget(save_button)
        layout.addLayout(toolbar)

        log = QPlainTextEdit()
        log.setReadOnly(True)
        log.setFont(QFont("Cascadia Mono", 10))
        log.setPlainText(
            "[--:--:--.---]  INFO  GUI 화면 준비 완료\n"
            "[--:--:--.---]  INFO  통신 기능은 아직 연결되지 않았습니다.\n"
            "[--:--:--.---]  INFO  ELF, WATCH, MEM_READ 기능은 다음 단계에서 구현합니다."
        )
        layout.addWidget(log, 1)
        return page

    def _build_status_bar(self) -> QStatusBar:
        status = QStatusBar()
        status.showMessage("준비 · UI 프로토타입 · 통신 기능 미연결")
        version = QLabel("UI v0.1")
        version.setObjectName("muted")
        status.addPermanentWidget(version)
        return status

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
