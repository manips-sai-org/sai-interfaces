The `sai2-interfaces-orientation` Element
====================================
The `sai2-interfaces-orientation` element allows for a more intuitive control of 
orientation by letting the user tune relative offsets to the current orientation.
Best understood using an example in sai2-examples, such as the 
[Puma example](https://github.com/manips-sai-org/sai2-examples/tree/master/01-non_redundant_arm).

## Usage
```
<sai2-interfaces-orientation key="..." refreshRate="">
</sai2-interfaces-orientation>
```

## Attributes
* `key`: Required. The Redis key that has a rotation matrix. If the Redis key
does not exist, an error will be thrown.
* `refreshRate`: Required. Refresh rate of the current rotation matrix.

## Example
We have provided a [script](./writekeys.py) that will generate a random
XYZ fixed-angle orientation and print it periodically. Our goal is to use
the `sai2-interfaces-orientation` module to observe that we can indeed specify
relative orientations and see the effect on the rotation matrix.

So, within the HTML body of the [provided HTML file] (./11-orientation.html),
we add the following HTML code:
```
<sai2-interfaces-orientation key="sai2::interfaces::tutorial::matrix_key" refreshRate="1">
</sai2-interfaces-display>
```

This means to use the `sai2::interfaces::tutorial::matrix_key` as our rotation matrix,
and to read it every second.

If we run the server, we should see something like this:

![](./initial.png)

From here, we can use the sliders to generate offsets:

![](./offset.png)


We also see the change in angles from the provided Python script:
```
(-1.437325441044437, 0.9203966989569864, -2.444464093700242)
(-1.437325441044437, 0.9203966989569864, -2.444464093700242)
(-1.437325441044437, 0.9203966989569864, -2.444464093700242)
(-0.43732544104443694, 0.9203966989569864, -2.444464093700242)
(-0.43732544104443694, 0.9203966989569864, -2.444464093700242)
(-0.43732544104443694, 0.9203966989569864, -2.444464093700242)
```