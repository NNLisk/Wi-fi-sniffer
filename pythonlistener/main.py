import threading

from listener import start_listener
from ui import render_ui


def main():
    listener_thread = threading.Thread(
        target=start_listener,
        daemon=True,
    )

    listener_thread.start()

    render_ui()


if __name__ == "__main__":
    main()