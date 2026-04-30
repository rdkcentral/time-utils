# time-utils

[![License](https://img.shields.io/badge/license-Apache%202.0-blue)](LICENSE)
[![Tests](https://img.shields.io/badge/tests-passing-brightgreen)](https://github.com/rdkcentral/time-utils/actions)

Centralized collection of time management utilities for RDK-based devices. This repository is the single home for libraries and tools that handle NTP daemon control, clock synchronization, and related time operations across the RDK platform.

## Table of Contents

- [Components](#components)
- [Contributing](#contributing)
- [License](#license)

## Components

- **[libchronyctl](libchronyctl/README.md)** — Lightweight C library for programmatic control of the `chronyd` NTP daemon over its Unix domain socket; replaces brittle `chronyc` subprocess calls with a typed in-process API.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines. All contributors must sign the RDK Contributor License Agreement (CLA).

## License

time-utils is licensed under the [Apache License, Version 2.0](LICENSE).

