import sys
from PyQt6.QtCore import Qt, QEasingCurve, QPropertyAnimation
from PyQt6.QtGui import QFont
from PyQt6.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
                             QLabel, QPlainTextEdit, QPushButton, QTableWidget)


class DatabaseManager(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Lightweight Database Manager")
        self.setGeometry(100, 100, 800, 600)
        self.setStyleSheet(self.main_stylesheet())
        
        self.conn = None
        self.cursor = None
        
        self.init_ui()
        
    def init_ui(self):
        # Central widget and layout
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QVBoxLayout(central_widget)
        
        # Header
        title_label = QLabel("Lightweight Database Manager")
        title_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        title_label.setStyleSheet("font-size: 24px; font-weight: bold; margin-bottom: 10px;")
        main_layout.addWidget(title_label)
        
        # Command input section
        main_layout.addWidget(QLabel("Enter SQL Commands:"))
        self.command_input = QPlainTextEdit()
        self.command_input.setStyleSheet("border: 2px solid #2A9D8F; border-radius: 5px; padding: 5px;")
        main_layout.addWidget(self.command_input)
        
        # Execution controls
        button_layout = QHBoxLayout()
        execute_button = QPushButton("Execute Command")
        execute_button.setStyleSheet(self.button_stylesheet())
        execute_button.clicked.connect(self.animate_button)
        execute_button.clicked.connect(self.execute_command)
        button_layout.addWidget(execute_button)
        
        clear_button = QPushButton("Clear Command")
        clear_button.setStyleSheet(self.button_stylesheet())
        clear_button.clicked.connect(self.animate_button)
        clear_button.clicked.connect(self.clear_command)
        button_layout.addWidget(clear_button)
        
        main_layout.addLayout(button_layout)
        
        # Results display section
        self.results_table = QTableWidget()
        self.results_table.setStyleSheet("border: 2px solid #264653; border-radius: 5px; background-color: #E9C46A;")
        main_layout.addWidget(self.results_table)
        
        # Status message
        self.status_message = QLabel()
        self.status_message.setStyleSheet("color: #F4A261; font-size: 16px;")
        main_layout.addWidget(self.status_message)
        
        # Footer (status bar)
        self.statusBar().showMessage("No database connected")
    
    def button_stylesheet(self):
        return """
        QPushButton {
            background-color: #2A9D8F;
            color: white;
            font-size: 16px;
            padding: 5px 15px;
            border-radius: 10px;
            border: 2px solid #264653;
        }
        QPushButton:hover {
            background-color: #E76F51;
        }
        QPushButton:pressed {
            background-color: #264653;
        }
        """
    
    def main_stylesheet(self):
        return """
        QMainWindow {
            background-color: #1D3557;
            color: white;
        }
        QLabel {
            color: white;
        }
        """
    
    def animate_button(self):
        sender = self.sender()
        anim = QPropertyAnimation(sender, b"geometry")
        anim.setDuration(150)
        anim.setStartValue(sender.geometry())
        anim.setEndValue(sender.geometry().adjusted(-5, -5, 5, 5))
        anim.setEasingCurve(QEasingCurve.Type.OutBounce)
        anim.start()
    
    def clear_command(self):
        self.command_input.clear()
    
    def execute_command(self):
        # Logic remains the same as before
        pass


if __name__ == '__main__':
    app = QApplication([])
    window = DatabaseManager()
    window.show()
    sys.exit(app.exec())
