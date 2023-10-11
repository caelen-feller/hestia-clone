from typing import Any, Dict
from pathlib import Path

import gitlab

class MockGitlab:
    def __init__(self,
        url: str | None = None,
        private_token: str | None = None,
        job_token: str | None = None,
        **kwargs: Any
        ) -> None:
        """
        Store all variables passed
        """
        self.url = url
        self.private_token = private_token
        self.job_token = job_token
        self.xargs = {}
        for key,val in kwargs: 
            self.xargs[key] = val
        
        self.projects = MockResourceManager(self, MockProject, "projects") 
        self.projects.create("testid")


class MockResource:
    def __init__(self, 
                 manager, 
                 id: str, 
                 attributes: Dict[str, str] | None = None) -> None:
        self.id = id
        self.manager = manager
        self.attributes = attributes

class MockProject(MockResource):
    def __init__(self, manager, id) -> None:
        super().__init__(manager, id)
        self.variables = MockResourceManager(manager.gl)
        self.generic_packages = MockGenericPackageManager(manager.gl)

class MockResourceManager:
    def __init__(self, gl: MockGitlab, 
                 resource_cls = MockResource, 
                 name: str | None = None) -> None:
        self.gl = gl
        self.name = name
        self.path = f"{gl.url}/{self.__class__.__name__ if self.name is None else self.name}"
        self._obj_store = {}
        self.resource_cls = resource_cls

    def create(self, id: str) -> None:
        if id not in self._obj_store:
            self._obj_store[id] = self.resource_cls(self, id)

    def get(self, id: str, lazy: bool = True) -> object | None:
        try:
            return self._obj_store[id]
        except KeyError:
            raise gitlab.GitlabGetError
        
    def update(self, id: str, value: Dict[str, str] | None = None) -> None:
        if id not in self._obj_store:
            self.create(id)
        self._obj_store[id] = self.resource_cls(self, id, attributes=value)


class MockGenericPackageManager(MockResourceManager):    
    def upload(self,
        package_name: str,
        package_version: str,
        file_name: str,
        path: str | Path):
        self._obj_store[package_name] = {
            "package_version": package_version,
            "file_name": file_name,
            "path": path
        }