
## CapsLock Artefacts

You can set up and use these artefacts either through Docker or manually in your host environment.

### Docker

#### Setup

Make sure you have Docker installed on your system.

You can pull the Docker image from Docker Hub:
```bash
./pull-docker
```

Alternatively, you can build the Docker image from the provided `Dockerfile`:
```
./build-docker
```

#### Usage

Build a Rust project:
```bash
./docker-build <path-to-rust-project>
```

Run a binary:
```bash
./docker-run <path-to-riscv-elf-binary>
```

NOTE: The Docker container will run with UID 1000. Please make sure that
the files are accessible to it. You can achieve this by running the following
in the host environment:
```
chown -R 1000:1000 <files>
```

#### Quick Tests

You can look at the `tests` directory for some quick tests of your setup.
You can use `./docker-test` to run both tests.

- `helloworld`: Prints out "Hello, world!"
- `violation`: Triggers an error in CapsLock "Attempting to use an invalid capability for load"

#### Experiments

Simply run `./docker-eval1` through `./docker-eval3` to run the respective experiments.
You can supply an optional argument to specify the number of CPU cores to use (default is 1).

Details about the experiments can be found in the `evaluations` directory.

### Host

#### Setup

##### Dependencies

Please check out `Dockerfile` for the dependencies required to build and run the artefacts.

##### Obtain the Source Code

The artefacts are submitted as patches.
To set them up, please run the setup.py script:
```
python3 setup.py
```

##### Build

To build the respective components, please run the scripts
`qemu.sh`, `llvm.sh`, and `rust.sh` (in this order) in
the `build` directory.


#### Usage

Assuming you are using bash, please do this first
```bash
source sourceme.sh
```

Then you can use `capslockbuild` and `capslockrun` to build and run
programs. The latter accepts an RISC-V ELF executable as argument.

#### Quick Tests

To run quick tests of your setup, look at the `tests` directory.

For example,
```bash
sourceme sourceme.sh
capslockbuild tests/helloworld
capslockrun tests/helloworld/target/riscv64gc-unknown-linux-gnu/debug/helloworld
```
should print "Hello, world!" to the console.

#### Experiments

To run the experiments without using Docker, make sure you have sourced `sourceme.sh`,
and then run the respective `just-run.sh` script in the corresponding experiment subdirectory in
the `evaluations` directory.
For details, please refer to the `README.md` files in the respective subdirectories.
