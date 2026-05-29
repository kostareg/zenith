{python3Packages}:

let
  py = python3Packages;

  git-me-the-url = py.buildPythonPackage rec {
    pname = "git-me-the-url";
    version = "2.2.0";
    format = "wheel";

    src = py.fetchPypi {
      pname = "git_me_the_url";
      inherit version format;
      dist = "py3";
      python = "py3";
      hash = "sha256-vXWd3efSHr4dhVWPANaCK3S35ZB63K65/E1yIDo0Jks=";
    };

    propagatedBuildInputs = [
      py.gitpython
    ];

    doCheck = false;
  };
in
py.buildPythonPackage rec {
  pname = "peakrdl-html";
  version = "2.12.2";
  format = "wheel";

  src = py.fetchPypi {
    pname = "peakrdl_html";
    inherit version format;
    dist = "py3";
    python = "py3";
    hash = "sha256-G3SkNXmQC0j7br8EaLkcUNIlaCjR+XUdqL/rLwiDcQw=";
  };

  propagatedBuildInputs = [
    py.systemrdl-compiler
    py.jinja2
    py.markdown
    py.python-markdown-math
    git-me-the-url
    py.peakrdl
  ];

  doCheck = false;
}
