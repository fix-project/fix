# Fix Viewer

## Running

If the `BUILD_VISUALIZER` cmake cache variable is set, e.g. with `-DBUILD_VISUALIZER` when configure the build directory,
`cmake` will attempt to build the viewer to a website. Then, running the `http-tester`, for example with
`http-tester 9090 ~/fix/build/src/tester/http-client/` will serve the viewer on the given port.

If developing on a remote machine, such as stagecast, and viewing from a local machine, like a laptop, ssh port forwarding
must be setup to access the http-tester's server. This can be accomplished with `ssh -L 9090:127.0.0.1:9090 stagecast.org`.

Visiting `127.0.0.1:9090` on your local machine should then show the viewer.

## Usage

The target handle indicates which object is under inspection. The progress tree shows the result of
an operation on the target handle. The handles in the middle of the operation arrow are "dependees"
of the targeted operation. That is, in order for the targeted operation to complete, it  required 
these intermediate calculations to complete first. The handle at the end of the operation arrow
is the "child" of running the targeted operation.

In the ancestry tree, parent handles point to handles below themselves. This is the dual of the 
"child" concept, as running some operation on these parent handles produced this child. Any child
can have multiple parents, all of which will show up in the ancestry tree. Each parent (with a specific
operation) only has one child because handles are content addressed and operations are deterministic.

## Building Locally

You can also build and run the viewer instead of through `cmake` as a native application. 

### Native

Make sure you are using the latest version of stable rust by running `rustup update`.

`cargo run --release`

On Linux you need to first run:

`sudo apt-get install libxcb-render0-dev libxcb-shape0-dev libxcb-xfixes0-dev libxkbcommon-dev libssl-dev`

On Fedora Rawhide you need to run:

`dnf install clang clang-devel clang-tools-extra libxkbcommon-devel pkg-config openssl-devel libxcb-devel gtk3-devel atk fontconfig-devel`

### Web

There are CORS restrictions that prevent making queries to Fixpoint's API from a page hosted on a different port number.
Therefore, serving the viewer on a local machine as opposed to from the `http-tester` prevents making requests to the API.

You can still build and preview the viewer, however, wih the steps below.

We use [Trunk](https://trunkrs.dev/) to build for web target.
1. Install Trunk with `cargo install --locked trunk`.
2. Run `trunk serve` to build and serve on `http://127.0.0.1:8080`. Trunk will rebuild automatically if you edit the project.
3. Open `http://127.0.0.1:8080/index.html#dev` in a browser. See the warning below.

> `assets/sw.js` script will try to cache our app, and loads the cached version when it cannot connect to server allowing your app to work offline (like PWA).
> appending `#dev` to `index.html` will skip this caching, allowing us to load the latest builds during development.

# Credit

The template used to create this viewer is at https://github.com/emilk/eframe_template. 
The unmodified portions of the template are primarily the README build instructions and the base html webpage.
