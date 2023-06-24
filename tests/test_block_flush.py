from math import ceil
from signal import SIGINT
from time import sleep
import pytest
from requests import Session, exceptions
from requests_futures.sessions import FuturesSession

from server import Server, server_port
from definitions import DYNAMIC_OUTPUT_CONTENT, SERVER_CONNECTION_OUTPUT
from utils import spawn_clients, generate_dynamic_headers, validate_out, validate_response_full, validate_response_full_with_dispatch


def test_sanity(server_port):
    with Server("./server", server_port, 1, 1, "bf") as server:
        sleep(0.1)
        with FuturesSession() as session1:
            future1 = session1.get(
                f"http://localhost:{server_port}/output.cgi?1")
            sleep(0.1)
            with Session() as session2:
                with pytest.raises(exceptions.ConnectionError):
                    session2.get(
                        f"http://localhost:{server_port}/output.cgi?1")
            response = future1.result()
            expected_headers = generate_dynamic_headers(123, 1, 0, 1)
            expected = DYNAMIC_OUTPUT_CONTENT.format(
                seconds="1.0")
            validate_response_full(response, expected_headers, expected)
        server.send_signal(SIGINT)
        out, err = server.communicate()
        expected = SERVER_CONNECTION_OUTPUT.format(
            filename=r"/output.cgi\?1")
        validate_out(out, err, expected)


@pytest.mark.parametrize("threads, queue, amount, dispatches",
                         [
                             (1, 2, 5, [0, 0.9, -1, 0, 1.3]),
                             (2, 4, 4, [0, 0, 0.8, 0.9]),
                             (2, 4, 8, [0, 0, 0.8, 0.9, -1, 0, 0, 1.5]),
                             (4, 4, 8, [0, 0, 0, 0, -1, 0, 0, 0]),
                             (4, 8, 8, [0, 0, 0, 0, 0.6, 0.7, 0.8, 0.9]),
                             (4, 8, 10, [0, 0, 0, 0, 0.6, 0.7, 0.8, 0.9, -1, 0]),
                         ])
def test_load(threads, queue, amount, dispatches, server_port):
    with Server("./server", server_port, threads, queue, "bf") as server:
        sleep(0.1)
        clients = spawn_clients(amount, server_port)

        expected_count = 0

        for i in range(amount):
            if i != queue:
                response = clients[i][1].result()
                clients[i][0].close()
                expected = DYNAMIC_OUTPUT_CONTENT.format(seconds=f"1.{i:0<1}")
                expected_headers = generate_dynamic_headers(123, (expected_count // threads) + 1, 0, (expected_count // threads) + 1)
                validate_response_full_with_dispatch(response, expected_headers, expected, dispatches[i])

                expected_count += 1

            else:
                with pytest.raises(exceptions.ConnectionError):
                    clients[i][1].result()

        server.send_signal(SIGINT)
        out, err = server.communicate()
        expected = "^" + ''.join([SERVER_CONNECTION_OUTPUT.format(
            filename=rf"/output.cgi\?1.{i}") + r"(?:.*[\r\n]+)*" for i in range(queue)])
        
        validate_out(out, err, expected)
