function [E, m, n] = readErr(fname)
% [E, m, n] = readErr(fname) reads the flow erros from a file
%
% E is the error matrix containing angular differences (in rad) for each
% image location. m is the mean angular error and n is the number of
% elements taken into account to calculate n.
% E may contain two distinct values -1e9 and 1e9. They indicate that either
% the left or the right image that were used to produce the error file
% contained a zero flow vector, while the other image did not. These
% occurenced lead to n < width * height in the calculation of m.

if isempty(fname)
    error('readErr: empty filename');
end

fd = fopen(fname, 'r');
if fd < 0
    error(sprintf('readErr: Could not open file %s', fname));
end

tag = fread(fd, 4, 'uint8=>char')';
if ~strcmp(tag, 'FERR')
    error(sprintf('readErr: Invalid FERR header in file %s', fname));
end

w = fread(fd, 1, 'int32');
h = fread(fd, 1, 'int32');
m = fread(fd, 1, 'float32');
n = fread(fd, 1, 'int32');
if w < 1 || w > 99999 || h < 1 || h > 99999
    error(sprintf('readErr: Invalid dimensions in file %s', fname));
end

E = fread(fd, inf, 'float32');
E = reshape(E, w, h);
E = E';

fclose(fd);